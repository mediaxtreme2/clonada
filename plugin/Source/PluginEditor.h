#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LicensePanel.h"

class ClonadaEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ClonadaEditor(ClonadaProcessor&);
    ~ClonadaEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateConnectionStatus();
    void populateModelList();

    ClonadaProcessor& processor_;

    // Header
    juce::Label titleLabel_;
    juce::Label statusLabel_;

    // Model selection
    juce::ComboBox modelSelector_;
    juce::TextButton browseButton_{"..."};
    juce::Label modelLabel_{"", "Voice Model:"};

    // Main controls
    juce::Slider pitchSlider_;
    juce::Slider formantSlider_;
    juce::Slider mixSlider_;
    juce::Slider inputGainSlider_;
    juce::Slider outputGainSlider_;

    juce::Label pitchLabel_{"", "Pitch"};
    juce::Label formantLabel_{"", "Formant"};
    juce::Label mixLabel_{"", "Mix"};
    juce::Label inputGainLabel_{"", "Input"};
    juce::Label outputGainLabel_{"", "Output"};

    // Mode & bypass
    juce::ComboBox modeSelector_;
    juce::ToggleButton bypassButton_{"Bypass"};
    juce::Label modeLabel_{"", "Mode:"};

    // License
    juce::TextButton licenseButton_{"License"};
    std::unique_ptr<LicensePanel> licensePanel_;
    bool showingLicense_ = false;

    // Metering
    float inputMeter_ = 0.0f;
    float outputMeter_ = 0.0f;

    // APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> formantAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach_;

    // Colors
    static constexpr juce::uint32 kBgDark    = 0xFF0a0a0c;
    static constexpr juce::uint32 kPanel      = 0xFF1a1a2e;
    static constexpr juce::uint32 kIndigo     = 0xFF6366f1;
    static constexpr juce::uint32 kCyan       = 0xFF06b6d4;
    static constexpr juce::uint32 kTextLight  = 0xFFe2e8f0;
    static constexpr juce::uint32 kTextDim    = 0xFF94a3b8;
    static constexpr juce::uint32 kGreen      = 0xFF22c55e;
    static constexpr juce::uint32 kRed        = 0xFFef4444;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClonadaEditor)
};
