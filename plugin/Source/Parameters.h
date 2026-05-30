#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs {
    static const juce::String PITCH         = "pitch";
    static const juce::String FORMANT       = "formant";
    static const juce::String MIX           = "mix";
    static const juce::String MODE          = "mode";
    static const juce::String MODEL_IDX     = "model_idx";
    static const juce::String BYPASS        = "bypass";
    static const juce::String INPUT_GAIN    = "input_gain";
    static const juce::String OUTPUT_GAIN   = "output_gain";
    static const juce::String INPUT_GRIT    = "input_grit";
    static const juce::String PITCH_TRACKER = "pitch_tracker";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ParamIDs::PITCH, 1}, "Pitch Shift", -24, 24, 0));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ParamIDs::FORMANT, 1}, "Formant Blend", -12, 12, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::MIX, 1}, "Identity Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamIDs::MODE, 1}, "Mode",
        juce::StringArray{"Low Latency", "High Quality"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ParamIDs::MODEL_IDX, 1}, "Model", 0, 99, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::BYPASS, 1}, "Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::INPUT_GAIN, 1}, "Input Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::OUTPUT_GAIN, 1}, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::INPUT_GRIT, 1}, "Input Grit",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamIDs::PITCH_TRACKER, 1}, "Pitch Tracker",
        juce::StringArray{"RMVPE", "CREPE"}, 0));

    return {params.begin(), params.end()};
}
