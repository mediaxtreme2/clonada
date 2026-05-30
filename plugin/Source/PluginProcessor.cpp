#include "PluginProcessor.h"
#include "PluginEditor.h"

ClonadaProcessor::ClonadaProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "PARAMETERS", createParameterLayout()) {

    // Try to auto-launch the Python engine sidecar
    if (licenseClient_.isActivated()) {
        engineLauncher_.launch();
        juce::Thread::sleep(1000);
    }
    bridge_.connect("tcp://127.0.0.1:5050");
}

ClonadaProcessor::~ClonadaProcessor() {
    bridge_.disconnect();
    engineLauncher_.shutdown();
}

void ClonadaProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentSampleRate_ = sampleRate;

    auto* modeParam = apvts_.getRawParameterValue(ParamIDs::MODE);
    bool highQuality = (modeParam && modeParam->load() > 0.5f);
    hopSize_ = highQuality ? 16384 : 256;

    size_t ringSize = static_cast<size_t>(hopSize_ * 4);
    inputRing_ = std::make_unique<LockFreeRingBuffer<float>>(ringSize);
    outputRing_ = std::make_unique<LockFreeRingBuffer<float>>(ringSize);

    swapInputBuffer_.resize(hopSize_, 0.0f);
    swapOutputBuffer_.resize(hopSize_, 0.0f);
    samplesCollected_ = 0;

    dryBuffer_.setSize(2, samplesPerBlock);

    updateLatency();
}

void ClonadaProcessor::releaseResources() {
    inputRing_.reset();
    outputRing_.reset();
}

void ClonadaProcessor::updateLatency() {
    currentLatencySamples_ = hopSize_;
    setLatencySamples(currentLatencySamples_);
}

void ClonadaProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    auto* bypassParam = apvts_.getRawParameterValue(ParamIDs::BYPASS);
    if (bypassParam && bypassParam->load() > 0.5f) return;

    auto* inputGainParam = apvts_.getRawParameterValue(ParamIDs::INPUT_GAIN);
    auto* outputGainParam = apvts_.getRawParameterValue(ParamIDs::OUTPUT_GAIN);
    auto* mixParam = apvts_.getRawParameterValue(ParamIDs::MIX);

    float inputGainDb = inputGainParam ? inputGainParam->load() : 0.0f;
    float outputGainDb = outputGainParam ? outputGainParam->load() : 0.0f;
    float mix = mixParam ? mixParam->load() : 1.0f;

    float inputGain = juce::Decibels::decibelsToGain(inputGainDb);
    float outputGain = juce::Decibels::decibelsToGain(outputGainDb);

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    // Apply input gain
    buffer.applyGain(inputGain);

    // Meter input level
    float inLevel = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        inLevel = std::max(inLevel, buffer.getMagnitude(ch, 0, numSamples));
    inputLevel_.store(inLevel);

    // Store dry signal for mix blending
    dryBuffer_.makeCopyOf(buffer, true);

    // Mono-sum input for AI processing (RVC works in mono)
    std::vector<float> monoInput(numSamples);
    if (numChannels >= 2) {
        const float* left = buffer.getReadPointer(0);
        const float* right = buffer.getReadPointer(1);
        for (int i = 0; i < numSamples; ++i)
            monoInput[i] = (left[i] + right[i]) * 0.5f;
    } else {
        std::memcpy(monoInput.data(), buffer.getReadPointer(0), numSamples * sizeof(float));
    }

    // Feed mono audio into input ring buffer
    if (inputRing_ && inputRing_->write(monoInput.data(), numSamples)) {
        samplesCollected_ += numSamples;
    }

    // When we have enough samples, submit a chunk to the engine
    if (samplesCollected_ >= hopSize_) {
        submitChunkForProcessing();
        samplesCollected_ -= hopSize_;
    }

    // Check if processed audio is available from engine
    if (bridge_.hasResponse()) {
        auto response = bridge_.consumeResponse();
        if (response.success && !response.audioData.empty()) {
            outputRing_->write(response.audioData.data(), response.audioData.size());
        }
    }

    // Read processed audio from output ring
    if (outputRing_ && outputRing_->availableToRead() >= (size_t)numSamples) {
        std::vector<float> processed(numSamples);
        outputRing_->read(processed.data(), numSamples);

        // Apply wet/dry mix and write to output buffer (stereo)
        for (int ch = 0; ch < numChannels; ++ch) {
            float* out = buffer.getWritePointer(ch);
            const float* dry = dryBuffer_.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                out[i] = dry[i] * (1.0f - mix) + processed[i] * mix;
            }
        }
    } else {
        // No processed audio yet - output dry signal (during initial latency fill)
        buffer.makeCopyOf(dryBuffer_, true);
    }

    // Apply output gain
    buffer.applyGain(outputGain);

    // Meter output level
    float outLevel = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        outLevel = std::max(outLevel, buffer.getMagnitude(ch, 0, numSamples));
    outputLevel_.store(outLevel);

    // Feed waveform display (downsample to every 16th sample for visualization)
    if (numChannels > 0) {
        const float* inPtr = dryBuffer_.getReadPointer(0);
        const float* outPtr = buffer.getReadPointer(0);
        for (int i = 0; i < numSamples; i += 16)
            waveformDisplay_.pushSample(inPtr[i], outPtr[i]);
    }
}

void ClonadaProcessor::submitChunkForProcessing() {
    if (!inputRing_ || inputRing_->availableToRead() < (size_t)hopSize_) return;

    std::vector<float> chunk(hopSize_);
    inputRing_->read(chunk.data(), hopSize_);

    auto* pitchParam = apvts_.getRawParameterValue(ParamIDs::PITCH);
    auto* formantParam = apvts_.getRawParameterValue(ParamIDs::FORMANT);
    auto* mixParam = apvts_.getRawParameterValue(ParamIDs::MIX);
    auto* modeParam = apvts_.getRawParameterValue(ParamIDs::MODE);
    auto* modelParam = apvts_.getRawParameterValue(ParamIDs::MODEL_IDX);

    ZmqBridge::SwapRequest request;
    request.audioData = std::move(chunk);
    request.sampleRate = static_cast<int>(currentSampleRate_);
    request.pitch = pitchParam ? static_cast<int>(pitchParam->load()) : 0;
    request.formant = formantParam ? static_cast<int>(formantParam->load()) : 0;
    request.mix = mixParam ? mixParam->load() : 1.0f;
    request.modelIndex = modelParam ? static_cast<int>(modelParam->load()) : 0;
    request.mode = (modeParam && modeParam->load() > 0.5f) ? "high_quality" : "low_latency";

    bridge_.submitSwapRequest(std::move(request));
}

juce::StringArray ClonadaProcessor::getAvailableModels() const {
    juce::StringArray models;
    if (modelsDir_.isDirectory()) {
        for (const auto& file : modelsDir_.findChildFiles(juce::File::findFiles, false, "*.pth")) {
            models.add(file.getFileNameWithoutExtension());
        }
    }
    return models;
}

void ClonadaProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    state.setProperty("modelPath", currentModelPath_, nullptr);
    state.setProperty("modelsDir", modelsDir_.getFullPathName(), nullptr);
    state.setProperty("runpodApiKey", runpodApiKey_, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ClonadaProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts_.state.getType())) {
        auto state = juce::ValueTree::fromXml(*xml);
        apvts_.replaceState(state);
        currentModelPath_ = state.getProperty("modelPath", "").toString();
        juce::String dirPath = state.getProperty("modelsDir", "").toString();
        if (dirPath.isNotEmpty())
            modelsDir_ = juce::File(dirPath);
        runpodApiKey_ = state.getProperty("runpodApiKey", "").toString();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ClonadaProcessor();
}
