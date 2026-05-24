#include "ZmqBridge.h"

#if defined(_WIN32)
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#endif

#include <zmq.h>

ZmqBridge::ZmqBridge() : juce::Thread("ZmqBridge") {
    zmqContext_ = zmq_ctx_new();
}

ZmqBridge::~ZmqBridge() {
    disconnect();
    if (zmqContext_) {
        zmq_ctx_destroy(zmqContext_);
        zmqContext_ = nullptr;
    }
}

void ZmqBridge::connect(const juce::String& endpoint) {
    endpoint_ = endpoint;
    state_.store(ConnectionState::Connecting);
    startThread(juce::Thread::Priority::normal);
}

void ZmqBridge::disconnect() {
    signalThreadShouldExit();
    stopThread(2000);

    if (zmqSocket_) {
        zmq_close(zmqSocket_);
        zmqSocket_ = nullptr;
    }
    state_.store(ConnectionState::Disconnected);
}

void ZmqBridge::submitSwapRequest(SwapRequest request) {
    juce::ScopedLock lock(requestLock_);
    pendingRequest_ = std::move(request);
    requestPending_.store(true);
}

ZmqBridge::SwapResponse ZmqBridge::consumeResponse() {
    juce::ScopedLock lock(responseLock_);
    responseReady_.store(false);
    return lastResponse_;
}

bool ZmqBridge::sendHealthCheck() {
    const char* msg = R"({"version":"1.0.0","command":"HEALTH"})";
    int rc = zmq_send(zmqSocket_, msg, strlen(msg), 0);
    if (rc < 0) return false;

    char buf[1024];
    rc = zmq_recv(zmqSocket_, buf, sizeof(buf) - 1, 0);
    if (rc < 0) return false;

    buf[rc] = '\0';
    return juce::String(buf).contains("SUCCESS");
}

void ZmqBridge::run() {
    zmqSocket_ = zmq_socket(zmqContext_, ZMQ_REQ);

    int timeout = 5000;
    zmq_setsockopt(zmqSocket_, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    zmq_setsockopt(zmqSocket_, ZMQ_SNDTIMEO, &timeout, sizeof(timeout));
    int linger = 0;
    zmq_setsockopt(zmqSocket_, ZMQ_LINGER, &linger, sizeof(linger));

    int rc = zmq_connect(zmqSocket_, endpoint_.toRawUTF8());
    if (rc != 0) {
        state_.store(ConnectionState::Error);
        if (onConnectionChanged_) onConnectionChanged_(ConnectionState::Error);
        return;
    }

    if (sendHealthCheck()) {
        state_.store(ConnectionState::Connected);
        if (onConnectionChanged_) onConnectionChanged_(ConnectionState::Connected);
    } else {
        state_.store(ConnectionState::Error);
        if (onConnectionChanged_) onConnectionChanged_(ConnectionState::Error);
    }

    while (!threadShouldExit()) {
        processQueue();
        wait(10);
    }
}

void ZmqBridge::processQueue() {
    if (!requestPending_.load()) return;

    SwapRequest req;
    {
        juce::ScopedLock lock(requestLock_);
        if (!requestPending_.load()) return;
        req = std::move(pendingRequest_);
        requestPending_.store(false);
    }

    // Build JSON command with base64-encoded audio
    juce::MemoryBlock audioBlock(req.audioData.data(), req.audioData.size() * sizeof(float));
    juce::String audioB64 = juce::Base64::toBase64(audioBlock.getData(), audioBlock.getSize());

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("version", "1.0.0");
    obj->setProperty("command", "SWAP");
    obj->setProperty("audio_b64", audioB64);
    obj->setProperty("sample_rate", req.sampleRate);
    obj->setProperty("pitch", req.pitch);
    obj->setProperty("formant", req.formant);
    obj->setProperty("mix", req.mix);
    obj->setProperty("model_index", req.modelIndex);
    obj->setProperty("mode", req.mode);
    obj->setProperty("num_samples", (int)req.audioData.size());

    juce::String json = juce::JSON::toString(juce::var(obj.get()));
    auto jsonUtf8 = json.toRawUTF8();

    int sendRc = zmq_send(zmqSocket_, jsonUtf8, strlen(jsonUtf8), 0);
    if (sendRc < 0) {
        juce::ScopedLock lock(responseLock_);
        lastResponse_ = {std::vector<float>(), false, "Send failed"};
        responseReady_.store(true);
        return;
    }

    // Receive response (may be large for audio data)
    zmq_msg_t respMsg;
    zmq_msg_init(&respMsg);

    int longTimeout = 30000;
    zmq_setsockopt(zmqSocket_, ZMQ_RCVTIMEO, &longTimeout, sizeof(longTimeout));

    int recvRc = zmq_msg_recv(&respMsg, zmqSocket_, 0);

    int normalTimeout = 5000;
    zmq_setsockopt(zmqSocket_, ZMQ_RCVTIMEO, &normalTimeout, sizeof(normalTimeout));

    if (recvRc < 0) {
        zmq_msg_close(&respMsg);
        juce::ScopedLock lock(responseLock_);
        lastResponse_ = {std::vector<float>(), false, "Receive timeout"};
        responseReady_.store(true);
        return;
    }

    juce::String respStr((const char*)zmq_msg_data(&respMsg), (size_t)zmq_msg_size(&respMsg));
    zmq_msg_close(&respMsg);

    auto parsed = juce::JSON::parse(respStr);
    if (!parsed.isObject()) {
        juce::ScopedLock lock(responseLock_);
        lastResponse_ = {std::vector<float>(), false, "Invalid JSON response"};
        responseReady_.store(true);
        return;
    }

    auto* respObj = parsed.getDynamicObject();
    juce::String status = respObj->getProperty("status").toString();

    if (status == "SUCCESS") {
        juce::String respAudioB64 = respObj->getProperty("audio_b64").toString();
        juce::MemoryBlock decoded;
        juce::Base64::convertFromBase64(decoded, respAudioB64);

        size_t numSamples = decoded.getSize() / sizeof(float);
        std::vector<float> audioOut(numSamples);
        std::memcpy(audioOut.data(), decoded.getData(), decoded.getSize());

        juce::ScopedLock lock(responseLock_);
        lastResponse_ = {std::move(audioOut), true, ""};
        responseReady_.store(true);
    } else {
        juce::String errMsg = respObj->getProperty("message").toString();
        juce::ScopedLock lock(responseLock_);
        lastResponse_ = {std::vector<float>(), false, errMsg};
        responseReady_.store(true);
    }
}
