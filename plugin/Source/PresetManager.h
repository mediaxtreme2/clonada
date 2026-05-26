#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class PresetManager {
public:
    struct Preset {
        juce::String name;
        juce::String category;
        juce::XmlElement* state = nullptr;
    };

    PresetManager(juce::AudioProcessorValueTreeState& apvts);
    ~PresetManager();

    void savePreset(const juce::String& name);
    bool loadPreset(int index);
    bool loadPreset(const juce::String& name);
    void deletePreset(int index);

    int getNumPresets() const { return presets_.size(); }
    juce::String getPresetName(int index) const;
    juce::StringArray getPresetNames() const;
    int getCurrentPresetIndex() const { return currentPreset_; }

    juce::File getPresetsDirectory() const;

private:
    void scanPresets();
    void loadFactoryPresets();

    juce::AudioProcessorValueTreeState& apvts_;
    juce::OwnedArray<juce::XmlElement> presets_;
    juce::StringArray presetNames_;
    int currentPreset_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
