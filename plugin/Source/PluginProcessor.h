#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "RingBuffer.h"
#include "ZmqBridge.h"
#include "Parameters.h"

class ClonadaProcessor : public juce::AudioProcessor {
public:
    ClonadaProcessor();
    ~ClonadaProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Clonada"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts_; }
    ZmqBridge& getBridge() { return bridge_; }

    // Model management
    void setModelPath(const juce::String& path) { currentModelPath_ = path; }
    juce::String getModelPath() const { return currentModelPath_; }
    void setModelsDirectory(const juce::File& dir) { modelsDir_ = dir; }
    juce::File getModelsDirectory() const { return modelsDir_; }
    juce::StringArray getAvailableModels() const;

    float getCurrentInputLevel() const { return inputLevel_.load(); }
    float getCurrentOutputLevel() const { return outputLevel_.load(); }
    bool isEngineConnected() const { return bridge_.getState() == ZmqBridge::ConnectionState::Connected; }

private:
    juce::AudioProcessorValueTreeState apvts_;
    ZmqBridge bridge_;

    // Lock-free audio pipeline
    std::unique_ptr<LockFreeRingBuffer<float>> inputRing_;
    std::unique_ptr<LockFreeRingBuffer<float>> outputRing_;

    // Processing state
    int hopSize_ = 256;
    int currentLatencySamples_ = 256;
    double currentSampleRate_ = 44100.0;
    bool processingActive_ = false;

    // Dry buffer for mix blending
    juce::AudioBuffer<float> dryBuffer_;
    std::vector<float> swapInputBuffer_;
    std::vector<float> swapOutputBuffer_;
    int samplesCollected_ = 0;

    // Metering
    std::atomic<float> inputLevel_{0.0f};
    std::atomic<float> outputLevel_{0.0f};

    juce::String currentModelPath_;
    juce::File modelsDir_;

    void updateLatency();
    void submitChunkForProcessing();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClonadaProcessor)
};
