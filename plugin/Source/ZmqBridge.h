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
    bool hasResponse() const { return responseReady_.load(); }
    SwapResponse consumeResponse();

    void setOnConnectionChanged(std::function<void(ConnectionState)> cb) { onConnectionChanged_ = std::move(cb); }

private:
    void run() override;
    void processQueue();
    bool sendHealthCheck();

    std::atomic<ConnectionState> state_{ConnectionState::Disconnected};
    std::atomic<bool> responseReady_{false};
    std::atomic<bool> requestPending_{false};

    juce::String endpoint_;
    SwapRequest pendingRequest_;
    SwapResponse lastResponse_;

    juce::CriticalSection requestLock_;
    juce::CriticalSection responseLock_;

    std::function<void(ConnectionState)> onConnectionChanged_;

    void* zmqContext_ = nullptr;
    void* zmqSocket_ = nullptr;
};
