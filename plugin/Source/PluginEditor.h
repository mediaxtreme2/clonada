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
    void switchTab(int tabIndex);
    void drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds, float level, juce::Colour colour);

    ClonadaProcessor& processor_;
    ClonadaLookAndFeel lnf_;
    PresetManager presetManager_;

    // Header
    juce::Label titleLabel_;
    juce::Label statusDotLabel_;
    juce::Label statusLabel_;
    juce::Label versionLabel_;

    // Tab bar
    juce::TextButton tabModeling_{"Modeling Studio"};
    juce::TextButton tabPerformance_{"Performance Bridge"};
    juce::TextButton tabSettings_{"Settings"};
    int activeTab_ = 0;

    // Preset selector
    juce::ComboBox presetSelector_;
    juce::TextButton savePresetButton_{"Save"};

    // ── Tab 0: Modeling Studio ──
    juce::Label identityBankLabel_{"", "IDENTITY BANK"};
    juce::ComboBox modelSelector_;
    juce::TextButton browseButton_{"..."};

    juce::Slider pitchSlider_;
    juce::Slider mixSlider_;
    juce::Slider formantSlider_;
    juce::Slider gritSlider_;

    juce::Label pitchLabel_{"", "PITCH SHIFT"};
    juce::Label mixLabel_{"", "IDENTITY MIX"};
    juce::Label formantLabel_{"", "FORMANT BLEND"};
    juce::Label gritLabel_{"", "INPUT GRIT"};

    juce::Label pitchTrackerLabel_{"", "PITCH TRACKER"};
    juce::ToggleButton rmvpeButton_{"RMVPE"};
    juce::ToggleButton crepeButton_{"CREPE"};

    juce::TextButton lowLatencyCard_{"Low Latency"};
    juce::TextButton highQualityCard_{"High Quality"};

    juce::TextButton applyButton_{"Apply Vocal Swap"};

    // ── Tab 1: Performance Bridge ──
    WaveformDisplay& waveformDisplay_;
    juce::Slider inputGainSlider_;
    juce::Slider outputGainSlider_;
    juce::Label inputGainLabel_{"", "IN"};
    juce::Label outputGainLabel_{"", "OUT"};
    juce::ToggleButton bypassButton_{"Bypass"};

    // ── Tab 2: Settings ──
    juce::TextButton licenseButton_{"Activate License"};
    juce::Label engineInfoLabel_;
    juce::Label buildInfoLabel_;

    // RunPod Cloud GPU
    juce::Label runpodSectionLabel_{"", "CLOUD GPU (RunPod)"};
    juce::Label runpodKeyLabel_{"", "API Key:"};
    juce::TextEditor runpodKeyInput_;
    juce::TextButton runpodSaveButton_{"Save Key"};
    juce::Label runpodStatusLabel_;

    // Overlays
    std::unique_ptr<LicensePanel> licensePanel_;
    bool showingLicense_ = false;
    std::unique_ptr<ModelBrowser> modelBrowser_;
    bool showingModelBrowser_ = false;

    // Metering
    float inputMeter_ = 0.0f;
    float outputMeter_ = 0.0f;
    float statusPulse_ = 0.0f;

    // APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> formantAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gritAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pitchTrackerAttach_;

    // Tab content component groups (for visibility toggling)
    std::vector<juce::Component*> modelingComponents_;
    std::vector<juce::Component*> performanceComponents_;
    std::vector<juce::Component*> settingsComponents_;

    // Dummy combobox to drive the mode APVTS attachment
    juce::ComboBox modeCombo_;
    // Dummy combobox for pitch tracker attachment
    juce::ComboBox pitchTrackerCombo_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClonadaEditor)
};
