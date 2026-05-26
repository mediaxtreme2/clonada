#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>

class WaveformDisplay : public juce::Component, private juce::Timer {
public:
    WaveformDisplay();

    void paint(juce::Graphics&) override;
    void pushSample(float inputSample, float outputSample);
    void setColours(juce::Colour input, juce::Colour output);

private:
    void timerCallback() override;

    static constexpr int kBufferSize = 512;
    std::array<float, kBufferSize> inputBuffer_{};
    std::array<float, kBufferSize> outputBuffer_{};
    std::atomic<int> writePos_{0};

    juce::Colour inputColour_{0xFF6366f1};
    juce::Colour outputColour_{0xFF06b6d4};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
