#include "PresetManager.h"
#include "Parameters.h"

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts) : apvts_(apvts) {
    getPresetsDirectory().createDirectory();
    loadFactoryPresets();
    scanPresets();
}

PresetManager::~PresetManager() = default;

juce::File PresetManager::getPresetsDirectory() const {
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appData.getChildFile("Clonada").getChildFile("Presets");
}

void PresetManager::loadFactoryPresets() {
    struct FactoryDef { const char* name; float pitch; float formant; float mix; float mode; };
    FactoryDef factories[] = {
        {"Default",             0.0f,  0.0f, 1.0f, 0.0f},
        {"Natural Male to Female", 12.0f, 4.0f, 0.85f, 1.0f},
        {"Natural Female to Male", -12.0f, -4.0f, 0.85f, 1.0f},
        {"Subtle Pitch Up",     3.0f,  0.0f, 0.9f, 0.0f},
        {"Subtle Pitch Down",   -3.0f, 0.0f, 0.9f, 0.0f},
        {"Radio Voice",         2.0f,  2.0f, 0.7f, 0.0f},
        {"Deep Bass",           -7.0f, -3.0f, 0.8f, 1.0f},
        {"Chipmunk",            24.0f, 0.0f, 1.0f, 0.0f},
        {"Harmony +5th",        7.0f,  0.0f, 0.5f, 1.0f},
        {"Gentle Blend",        0.0f,  0.0f, 0.4f, 1.0f},
    };

    for (auto& f : factories) {
        auto dir = getPresetsDirectory();
        auto file = dir.getChildFile(juce::String(f.name) + ".xml");
        if (file.existsAsFile()) continue;

        auto xml = std::make_unique<juce::XmlElement>("ClonadaPreset");
        xml->setAttribute("name", f.name);
        xml->setAttribute("category", "Factory");
        xml->setAttribute(ParamIDs::PITCH, (double)f.pitch);
        xml->setAttribute(ParamIDs::FORMANT, (double)f.formant);
        xml->setAttribute(ParamIDs::MIX, (double)f.mix);
        xml->setAttribute(ParamIDs::MODE, (double)f.mode);
        xml->setAttribute(ParamIDs::INPUT_GAIN, 0.0);
        xml->setAttribute(ParamIDs::OUTPUT_GAIN, 0.0);
        xml->setAttribute(ParamIDs::BYPASS, 0.0);
        xml->writeTo(file);
    }
}

void PresetManager::scanPresets() {
    presets_.clear();
    presetNames_.clear();

    auto dir = getPresetsDirectory();
    for (auto& file : dir.findChildFiles(juce::File::findFiles, false, "*.xml")) {
        auto xml = juce::XmlDocument::parse(file);
        if (xml && xml->hasTagName("ClonadaPreset")) {
            presetNames_.add(xml->getStringAttribute("name", file.getFileNameWithoutExtension()));
            presets_.add(xml.release());
        }
    }
}

void PresetManager::savePreset(const juce::String& name) {
    auto xml = std::make_unique<juce::XmlElement>("ClonadaPreset");
    xml->setAttribute("name", name);
    xml->setAttribute("category", "User");

    auto state = apvts_.copyState();
    for (int i = 0; i < state.getNumChildren(); ++i) {
        auto child = state.getChild(i);
        auto id = child.getProperty("id").toString();
        auto value = child.getProperty("value");
        xml->setAttribute(id, value.toString());
    }

    auto file = getPresetsDirectory().getChildFile(name + ".xml");
    xml->writeTo(file);
    scanPresets();
}

bool PresetManager::loadPreset(int index) {
    if (index < 0 || index >= presets_.size()) return false;

    auto* xml = presets_[index];
    const juce::String paramIds[] = {
        ParamIDs::PITCH, ParamIDs::FORMANT, ParamIDs::MIX,
        ParamIDs::INPUT_GAIN, ParamIDs::OUTPUT_GAIN,
        ParamIDs::MODE, ParamIDs::BYPASS
    };

    for (auto& id : paramIds) {
        if (xml->hasAttribute(id)) {
            if (auto* param = apvts_.getParameter(id))
                param->setValueNotifyingHost(
                    param->convertTo0to1(static_cast<float>(xml->getDoubleAttribute(id))));
        }
    }

    currentPreset_ = index;
    return true;
}

bool PresetManager::loadPreset(const juce::String& name) {
    for (int i = 0; i < presetNames_.size(); ++i) {
        if (presetNames_[i] == name)
            return loadPreset(i);
    }
    return false;
}

void PresetManager::deletePreset(int index) {
    if (index < 0 || index >= presets_.size()) return;

    auto name = presetNames_[index];
    auto file = getPresetsDirectory().getChildFile(name + ".xml");
    file.deleteFile();
    scanPresets();
    if (currentPreset_ == index) currentPreset_ = -1;
}

juce::String PresetManager::getPresetName(int index) const {
    return presetNames_[index];
}

juce::StringArray PresetManager::getPresetNames() const {
    return presetNames_;
}
