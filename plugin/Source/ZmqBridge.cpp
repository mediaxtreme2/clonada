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

void ZmqBridge::submitLoadModel(const juce::String& modelPath) {
    juce::ScopedLock lock(modelLock_);
    pendingModelPath_ = modelPath;
    modelLoadPending_.store(true);
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

juce::String ZmqBridge::sendCommandSync(const juce::String& jsonCommand, int timeoutMs) {
    juce::ScopedLock lock(syncLock_);

    if (!zmqContext_ || state_.load() != ConnectionState::Connected)
        return R"({"status":"ERROR","message":"Not connected"})";

    void* syncSocket = zmq_socket(zmqContext_, ZMQ_REQ);
    if (!syncSocket)
        return R"({"status":"ERROR","message":"Socket creation failed"})";

    zmq_setsockopt(syncSocket, ZMQ_SNDTIMEO, &timeoutMs, sizeof(timeoutMs));
    zmq_setsockopt(syncSocket, ZMQ_RCVTIMEO, &timeoutMs, sizeof(timeoutMs));
    int linger = 0;
    zmq_setsockopt(syncSocket, ZMQ_LINGER, &linger, sizeof(linger));

    int rc = zmq_connect(syncSocket, endpoint_.toRawUTF8());
    if (rc != 0) {
        zmq_close(syncSocket);
        return R"({"status":"ERROR","message":"Connect failed"})";
    }

    auto utf8 = jsonCommand.toRawUTF8();
    rc = zmq_send(syncSocket, utf8, strlen(utf8), 0);
    if (rc < 0) {
        zmq_close(syncSocket);
        return R"({"status":"ERROR","message":"Send failed"})";
    }

    zmq_msg_t respMsg;
    zmq_msg_init(&respMsg);
    rc = zmq_msg_recv(&respMsg, syncSocket, 0);
    if (rc < 0) {
        zmq_msg_close(&respMsg);
        zmq_close(syncSocket);
        return R"({"status":"ERROR","message":"Receive timeout"})";
    }

    juce::String result((const char*)zmq_msg_data(&respMsg), (size_t)zmq_msg_size(&respMsg));
    zmq_msg_close(&respMsg);
    zmq_close(syncSocket);
    return result;
}

void ZmqBridge::processQueue() {
    // Handle model load requests first
    if (modelLoadPending_.load()) {
        juce::String modelPath;
        {
            juce::ScopedLock lock(modelLock_);
            modelPath = pendingModelPath_;
            modelLoadPending_.store(false);
        }

        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("version", "1.0.0");
        obj->setProperty("command", "LOAD_MODEL");
        obj->setProperty("model_path", modelPath);

        juce::String json = juce::JSON::toString(juce::var(obj.get()));
        auto jsonUtf8 = json.toRawUTF8();

        int sendRc = zmq_send(zmqSocket_, jsonUtf8, strlen(jsonUtf8), 0);
        if (sendRc >= 0) {
            char buf[4096];
            int recvRc = zmq_recv(zmqSocket_, buf, sizeof(buf) - 1, 0);
            if (recvRc > 0) {
                buf[recvRc] = '\0';
                juce::String resp(buf);
                if (resp.contains("SUCCESS")) {
                    modelLoaded_.store(true);
                    loadedModelName_ = juce::File(modelPath).getFileNameWithoutExtension();
                }
            }
        }
    }

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
        juce::MemoryOutputStream mos;
        juce::Base64::convertFromBase64(mos, respAudioB64);

        size_t numSamples = mos.getDataSize() / sizeof(float);
        std::vector<float> audioOut(numSamples);
        std::memcpy(audioOut.data(), mos.getData(), mos.getDataSize());

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
