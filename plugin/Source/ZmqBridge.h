#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>
#include <functional>

class ZmqBridge : private juce::Thread {
public:
    enum class ConnectionState { Disconnected, Connecting, Connected, Error };

    struct SwapRequest {
        std::vector<float> audioData;
        int sampleRate;
        int pitch;
        int formant;
        float mix;
        int modelIndex;
        juce::String mode;
    };

    struct SwapResponse {
        std::vector<float> audioData;
        bool success;
        juce::String error;
    };

    ZmqBridge();
    ~ZmqBridge() override;

    void connect(const juce::String& endpoint = "tcp://127.0.0.1:5050");
    void disconnect();
    ConnectionState getState() const { return state_.load(); }

    void submitSwapRequest(SwapRequest request);
    void submitLoadModel(const juce::String& modelPath);
    bool hasResponse() const { return responseReady_.load(); }
    SwapResponse consumeResponse();

    bool isModelLoaded() const { return modelLoaded_.load(); }
    juce::String getLoadedModelName() const { return loadedModelName_; }

    void setOnConnectionChanged(std::function<void(ConnectionState)> cb) { onConnectionChanged_ = std::move(cb); }

    juce::String sendCommandSync(const juce::String& jsonCommand, int timeoutMs = 1800000);

private:
    void run() override;
    void processQueue();
    bool sendHealthCheck();

    std::atomic<ConnectionState> state_{ConnectionState::Disconnected};
    std::atomic<bool> responseReady_{false};
    std::atomic<bool> requestPending_{false};
    std::atomic<bool> modelLoadPending_{false};
    std::atomic<bool> modelLoaded_{false};

    juce::String endpoint_;
    SwapRequest pendingRequest_;
    SwapResponse lastResponse_;
    juce::String pendingModelPath_;
    juce::String loadedModelName_;
    juce::CriticalSection modelLock_;

    juce::CriticalSection requestLock_;
    juce::CriticalSection responseLock_;

    std::function<void(ConnectionState)> onConnectionChanged_;

    void* zmqContext_ = nullptr;
    void* zmqSocket_ = nullptr;

    juce::CriticalSection syncLock_;
};
