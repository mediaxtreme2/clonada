#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LicensePanel.h"
#include "ClonadaLookAndFeel.h"
#include "WaveformDisplay.h"
#include "PresetManager.h"
#include "ModelBrowser.h"

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
    void populatePresetList();

    ClonadaProcessor& processor_;
    ClonadaLookAndFeel lnf_;
    PresetManager presetManager_;

    // Header
    juce::Label titleLabel_;
    juce::Label statusLabel_;
    juce::Label versionLabel_;

    // Preset selector
    juce::ComboBox presetSelector_;
    juce::TextButton savePresetButton_{"Save"};

    // Model selection
    juce::ComboBox modelSelector_;
    juce::TextButton browseButton_{"..."};
    juce::Label modelLabel_{"", "VOICE MODEL"};

    // Main controls
    juce::Slider pitchSlider_;
    juce::Slider formantSlider_;
    juce::Slider mixSlider_;
    juce::Slider inputGainSlider_;
    juce::Slider outputGainSlider_;

    juce::Label pitchLabel_{"", "PITCH"};
    juce::Label formantLabel_{"", "FORMANT"};
    juce::Label mixLabel_{"", "MIX"};
    juce::Label inputGainLabel_{"", "IN"};
    juce::Label outputGainLabel_{"", "OUT"};

    // Mode & bypass
    juce::ComboBox modeSelector_;
    juce::ToggleButton bypassButton_{"Bypass"};
    juce::Label modeLabel_{"", "MODE"};

    // License
    juce::TextButton licenseButton_{"License"};
    std::unique_ptr<LicensePanel> licensePanel_;
    bool showingLicense_ = false;

    // Model browser overlay
    std::unique_ptr<ModelBrowser> modelBrowser_;
    bool showingModelBrowser_ = false;

    // Waveform (owned by processor, displayed here)
    WaveformDisplay& waveformDisplay_;

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

    void drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds, float level, juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClonadaEditor)
};
