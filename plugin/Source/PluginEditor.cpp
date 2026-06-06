#include "PluginEditor.h"

using LnF = ClonadaLookAndFeel;

ClonadaEditor::ClonadaEditor(ClonadaProcessor& p)
    : AudioProcessorEditor(&p), processor_(p),
      presetManager_(p.getAPVTS()),
      waveformDisplay_(p.getWaveformDisplay()) {

    setLookAndFeel(&lnf_);
    setSize(820, 680);
    setResizable(false, false);

    // ── Header ──
    titleLabel_.setVisible(false);

    statusDotLabel_.setText(juce::CharPointer_UTF8("\xe2\x97\x8f"), juce::dontSendNotification);
    statusDotLabel_.setFont(juce::FontOptions(13.0f));
    statusDotLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kRed));
    addAndMakeVisible(statusDotLabel_);

    statusLabel_.setText("Engine Offline", juce::dontSendNotification);
    statusLabel_.setFont(juce::FontOptions(13.0f));
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kTextGrey));
    addAndMakeVisible(statusLabel_);

    versionLabel_.setText("v1.7.8", juce::dontSendNotification);
    versionLabel_.setFont(juce::FontOptions(12.0f));
    versionLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kTextDark));
    versionLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(versionLabel_);

    // ── Preset selector ──
    presetSelector_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(LnF::kSlatePanel));
    presetSelector_.setColour(juce::ComboBox::textColourId, juce::Colour(LnF::kTextWhite));
    presetSelector_.setColour(juce::ComboBox::outlineColourId, juce::Colour(LnF::kBorder));
    presetSelector_.onChange = [this] {
        presetManager_.loadPreset(presetSelector_.getSelectedId() - 1);
    };
    addAndMakeVisible(presetSelector_);
    populatePresetList();

    savePresetButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(LnF::kSlatePanel));
    savePresetButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(LnF::kTextGrey));
    savePresetButton_.onClick = [this] {
        auto name = juce::String("User Preset ") + juce::String(presetManager_.getNumPresets() + 1);
        presetManager_.savePreset(name);
        populatePresetList();
    };
    addAndMakeVisible(savePresetButton_);

    // ── Tab bar ──
    auto setupTabButton = [this](juce::TextButton& btn, int idx) {
        btn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOffId, juce::Colour(LnF::kTextGrey));
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOnId, juce::Colour(LnF::kCyanGlow));
        btn.onClick = [this, idx] { switchTab(idx); };
        addAndMakeVisible(btn);
    };
    setupTabButton(tabModeling_, 0);
    setupTabButton(tabPerformance_, 1);
    setupTabButton(tabSettings_, 2);

    // ── Knob setup ──
    auto setupKnob = [this](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(LnF::kCyanGlow));
        s.setColour(juce::Slider::thumbColourId, juce::Colour(LnF::kCyanGlow));
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(LnF::kTextWhite));
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(s);
    };

    auto setupKnobLabel = [this](juce::Label& l) {
        l.setColour(juce::Label::textColourId, juce::Colour(LnF::kTextGrey));
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        addAndMakeVisible(l);
    };

    setupKnob(pitchSlider_);
    setupKnob(mixSlider_);
    setupKnob(formantSlider_);
    setupKnob(gritSlider_);
    setupKnobLabel(pitchLabel_);
    setupKnobLabel(mixLabel_);
    setupKnobLabel(formantLabel_);
    setupKnobLabel(gritLabel_);

    // ── Identity Bank (model selector) ──
    identityBankLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kTextGrey));
    identityBankLabel_.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(identityBankLabel_);

    modelSelector_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(LnF::kSlatePanel));
    modelSelector_.setColour(juce::ComboBox::textColourId, juce::Colour(LnF::kTextWhite));
    modelSelector_.setColour(juce::ComboBox::outlineColourId, juce::Colour(LnF::kCyanGlow).withAlpha(0.3f));
    modelSelector_.onChange = [this] {
        int idx = modelSelector_.getSelectedId() - 1;
        auto models = processor_.getAvailableModels();
        if (idx >= 0 && idx < models.size()) {
            auto modelFile = processor_.getModelsDirectory().getChildFile(models[idx] + ".pth");
            processor_.setModelPath(modelFile.getFullPathName());
            processor_.getBridge().submitLoadModel(modelFile.getFullPathName());
        }
    };
    addAndMakeVisible(modelSelector_);
    populateModelList();

    browseButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(LnF::kCyanGlow).withAlpha(0.15f));
    browseButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(LnF::kCyanGlow));
    browseButton_.onClick = [this] {
        showingModelBrowser_ = !showingModelBrowser_;
        if (showingModelBrowser_) {
            modelBrowser_ = std::make_unique<ModelBrowser>(processor_);
            modelBrowser_->onModelSelected = [this] { populateModelList(); };
            modelBrowser_->onClose = [this] {
                showingModelBrowser_ = false;
                modelBrowser_.reset();
                populateModelList();
                repaint();
            };
            addAndMakeVisible(*modelBrowser_);
            modelBrowser_->setBounds(getLocalBounds().reduced(40, 60));
        } else {
            modelBrowser_.reset();
            populateModelList();
        }
        repaint();
    };
    addAndMakeVisible(browseButton_);

    // ── Pitch Tracker radio buttons ──
    pitchTrackerLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kTextGrey));
    pitchTrackerLabel_.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(pitchTrackerLabel_);

    auto setupRadio = [this](juce::ToggleButton& btn, int /*groupId*/) {
        btn.setRadioGroupId(1001);
        btn.setColour(juce::ToggleButton::textColourId, juce::Colour(LnF::kTextWhite));
        btn.setColour(juce::ToggleButton::tickColourId, juce::Colour(LnF::kCyanGlow));
        addAndMakeVisible(btn);
    };
    setupRadio(rmvpeButton_, 1001);
    setupRadio(crepeButton_, 1001);
    rmvpeButton_.setToggleState(true, juce::dontSendNotification);

    // Hidden combo for APVTS binding
    pitchTrackerCombo_.addItem("RMVPE", 1);
    pitchTrackerCombo_.addItem("CREPE", 2);
    pitchTrackerCombo_.setVisible(false);
    addChildComponent(pitchTrackerCombo_);

    rmvpeButton_.onClick = [this] { pitchTrackerCombo_.setSelectedId(1, juce::sendNotification); };
    crepeButton_.onClick = [this] { pitchTrackerCombo_.setSelectedId(2, juce::sendNotification); };

    // ── Mode cards ──
    auto setupModeCard = [this](juce::TextButton& btn) {
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(LnF::kSlatePanel));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colour(LnF::kTextGrey));
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(LnF::kCyanGlow).withAlpha(0.15f));
        btn.setColour(juce::TextButton::textColourOnId, juce::Colour(LnF::kCyanGlow));
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(1002);
        addAndMakeVisible(btn);
    };
    setupModeCard(lowLatencyCard_);
    setupModeCard(highQualityCard_);
    lowLatencyCard_.setToggleState(true, juce::dontSendNotification);

    // Hidden combo for mode APVTS binding
    modeCombo_.addItem("Low Latency", 1);
    modeCombo_.addItem("High Quality", 2);
    modeCombo_.setVisible(false);
    addChildComponent(modeCombo_);

    lowLatencyCard_.onClick = [this] {
        modeCombo_.setSelectedId(1, juce::sendNotification);
    };
    highQualityCard_.onClick = [this] {
        modeCombo_.setSelectedId(2, juce::sendNotification);
    };

    // ── Apply Vocal Swap button ──
    applyButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(LnF::kCyanGlow));
    applyButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(LnF::kObsidian));
    applyButton_.onClick = [this] {
        if (!processor_.isEngineConnected()) {
            processor_.launchEngine();
        }
    };
    addAndMakeVisible(applyButton_);

    // ── Performance Bridge tab ──
    auto setupFader = [this](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 14);
        s.setColour(juce::Slider::trackColourId, juce::Colour(LnF::kCyanGlow));
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(LnF::kTextWhite));
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(s);
    };
    auto setupFaderLabel = [this](juce::Label& l) {
        l.setColour(juce::Label::textColourId, juce::Colour(LnF::kTextGrey));
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        addAndMakeVisible(l);
    };
    setupFader(inputGainSlider_);
    setupFader(outputGainSlider_);
    setupFaderLabel(inputGainLabel_);
    setupFaderLabel(outputGainLabel_);
    addAndMakeVisible(waveformDisplay_);

    bypassButton_.setColour(juce::ToggleButton::textColourId, juce::Colour(LnF::kTextWhite));
    bypassButton_.setColour(juce::ToggleButton::tickColourId, juce::Colour(LnF::kRed));
    addAndMakeVisible(bypassButton_);

    // ── Settings tab ──
    licenseButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(LnF::kCyanGlow).withAlpha(0.15f));
    licenseButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(LnF::kCyanGlow));
    licenseButton_.onClick = [this] {
        showingLicense_ = !showingLicense_;
        if (showingLicense_) {
            licensePanel_ = std::make_unique<LicensePanel>(processor_.getLicenseClient());
            licensePanel_->onLicenseActivated = [this] { processor_.launchEngine(); };
            licensePanel_->onClose = [this] {
                showingLicense_ = false;
                licensePanel_.reset();
                repaint();
            };
            addAndMakeVisible(*licensePanel_);
            licensePanel_->setBounds(getLocalBounds().reduced(160, 130));
        } else {
            licensePanel_.reset();
        }
        repaint();
    };
    addAndMakeVisible(licenseButton_);

    engineInfoLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kTextGrey));
    engineInfoLabel_.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(engineInfoLabel_);

    buildInfoLabel_.setText("Clonada AI Vocal Suite v1.7.8\nmediaXtreme LLC", juce::dontSendNotification);
    buildInfoLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kTextGrey));
    buildInfoLabel_.setFont(juce::FontOptions(14.0f));
    addAndMakeVisible(buildInfoLabel_);

    // ── RunPod Cloud GPU ──
    runpodSectionLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kCyanGlow));
    runpodSectionLabel_.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    addAndMakeVisible(runpodSectionLabel_);

    runpodKeyLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kTextGrey));
    runpodKeyLabel_.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(runpodKeyLabel_);

    runpodKeyInput_.setFont(juce::FontOptions(13.0f));
    runpodKeyInput_.setTextToShowWhenEmpty("rpa_XXXXXXXXXXXXXXXXXXXXXXXX", juce::Colour(LnF::kTextDark));
    runpodKeyInput_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(LnF::kSlatePanel));
    runpodKeyInput_.setColour(juce::TextEditor::textColourId, juce::Colour(LnF::kTextWhite));
    runpodKeyInput_.setColour(juce::TextEditor::outlineColourId, juce::Colour(LnF::kBorder));
    runpodKeyInput_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(LnF::kCyanGlow));
    runpodKeyInput_.setPasswordCharacter(0x2022);
    auto savedKey = processor_.getRunPodApiKey();
    if (savedKey.isNotEmpty())
        runpodKeyInput_.setText(savedKey, juce::dontSendNotification);
    addAndMakeVisible(runpodKeyInput_);

    runpodSaveButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(LnF::kCyanGlow).withAlpha(0.15f));
    runpodSaveButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(LnF::kCyanGlow));
    runpodSaveButton_.onClick = [this] {
        auto key = runpodKeyInput_.getText().trim();
        processor_.setRunPodApiKey(key);
        if (key.isNotEmpty()) {
            runpodStatusLabel_.setText("Key saved", juce::dontSendNotification);
            runpodStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kGreen));
        } else {
            runpodStatusLabel_.setText("Key cleared", juce::dontSendNotification);
            runpodStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kTextDim));
        }
    };
    addAndMakeVisible(runpodSaveButton_);

    runpodStatusLabel_.setFont(juce::FontOptions(12.0f));
    runpodStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kTextDim));
    if (savedKey.isNotEmpty()) {
        runpodStatusLabel_.setText("Key configured", juce::dontSendNotification);
        runpodStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kGreen));
    } else {
        runpodStatusLabel_.setText("Not configured", juce::dontSendNotification);
    }
    addAndMakeVisible(runpodStatusLabel_);

    // ── Group components by tab ──
    modelingComponents_ = {
        &identityBankLabel_, &modelSelector_, &browseButton_,
        &pitchSlider_, &mixSlider_, &formantSlider_, &gritSlider_,
        &pitchLabel_, &mixLabel_, &formantLabel_, &gritLabel_,
        &pitchTrackerLabel_, &rmvpeButton_, &crepeButton_,
        &lowLatencyCard_, &highQualityCard_, &applyButton_
    };
    performanceComponents_ = {
        &waveformDisplay_, &inputGainSlider_, &outputGainSlider_,
        &inputGainLabel_, &outputGainLabel_, &bypassButton_
    };
    settingsComponents_ = {
        &licenseButton_, &engineInfoLabel_, &buildInfoLabel_,
        &runpodSectionLabel_, &runpodKeyLabel_, &runpodKeyInput_,
        &runpodSaveButton_, &runpodStatusLabel_
    };

    // ── APVTS Attachments ──
    pitchAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::PITCH, pitchSlider_);
    formantAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::FORMANT, formantSlider_);
    mixAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::MIX, mixSlider_);
    gritAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::INPUT_GRIT, gritSlider_);
    inputGainAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::INPUT_GAIN, inputGainSlider_);
    outputGainAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::OUTPUT_GAIN, outputGainSlider_);
    modeAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.getAPVTS(), ParamIDs::MODE, modeCombo_);
    bypassAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.getAPVTS(), ParamIDs::BYPASS, bypassButton_);
    pitchTrackerAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.getAPVTS(), ParamIDs::PITCH_TRACKER, pitchTrackerCombo_);

    switchTab(0);
    startTimerHz(30);
}

ClonadaEditor::~ClonadaEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void ClonadaEditor::parentHierarchyChanged() {
    if (auto* top = getTopLevelComponent()) {
        if (auto* window = dynamic_cast<juce::DocumentWindow*>(top))
            window->setName("");
        else
            top->setName("");
    }
}


void ClonadaEditor::switchTab(int tabIndex) {
    activeTab_ = tabIndex;

    for (auto* c : modelingComponents_)    c->setVisible(tabIndex == 0);
    for (auto* c : performanceComponents_) c->setVisible(tabIndex == 1);
    for (auto* c : settingsComponents_)    c->setVisible(tabIndex == 2);

    tabModeling_.setToggleState(tabIndex == 0, juce::dontSendNotification);
    tabPerformance_.setToggleState(tabIndex == 1, juce::dontSendNotification);
    tabSettings_.setToggleState(tabIndex == 2, juce::dontSendNotification);

    resized();
    repaint();
}

void ClonadaEditor::timerCallback() {
    updateConnectionStatus();
    inputMeter_ = processor_.getCurrentInputLevel();
    outputMeter_ = processor_.getCurrentOutputLevel();
    statusPulse_ += 0.08f;
    if (statusPulse_ > juce::MathConstants<float>::twoPi)
        statusPulse_ -= juce::MathConstants<float>::twoPi;
    repaint();
}

void ClonadaEditor::updateConnectionStatus() {
    auto& bridge = processor_.getBridge();
    if (processor_.isEngineConnected()) {
        if (bridge.isModelLoaded()) {
            statusLabel_.setText("Voice: " + bridge.getLoadedModelName(), juce::dontSendNotification);
            statusDotLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kCyanGlow));
        } else {
            statusLabel_.setText("Engine Online", juce::dontSendNotification);
            statusDotLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kGreen));
        }
    } else {
        statusLabel_.setText("Engine Offline", juce::dontSendNotification);
        statusDotLabel_.setColour(juce::Label::textColourId, juce::Colour(LnF::kRed));
    }

    engineInfoLabel_.setText(
        juce::String("Engine: ") + (processor_.isEngineConnected() ? "Connected" : "Disconnected") +
        "\nLatency: " + juce::String(processor_.getLatencySamples()) + " samples" +
        "\nSample Rate: " + juce::String((int)processor_.getSampleRate()) + " Hz",
        juce::dontSendNotification);
}

void ClonadaEditor::populateModelList() {
    modelSelector_.clear();
    auto models = processor_.getAvailableModels();
    for (int i = 0; i < models.size(); ++i)
        modelSelector_.addItem(models[i], i + 1);
    if (models.size() > 0)
        modelSelector_.setSelectedId(1);
}

void ClonadaEditor::populatePresetList() {
    presetSelector_.clear();
    auto names = presetManager_.getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        presetSelector_.addItem(names[i], i + 1);
    if (names.size() > 0)
        presetSelector_.setSelectedId(1, juce::dontSendNotification);
}

void ClonadaEditor::drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                                float level, juce::Colour colour) {
    g.setColour(juce::Colour(0xFF0A0A0A));
    g.fillRoundedRectangle(bounds, 3.0f);

    float clampedLevel = juce::jlimit(0.0f, 1.0f, level);
    float fillH = clampedLevel * bounds.getHeight();

    auto fillBounds = bounds.withTop(bounds.getBottom() - fillH);
    g.setColour(colour.withAlpha(0.12f));
    g.fillRoundedRectangle(fillBounds.expanded(2.0f, 0.0f), 3.0f);

    juce::ColourGradient grad(colour, bounds.getCentreX(), bounds.getBottom(),
                               colour.brighter(0.3f), bounds.getCentreX(), fillBounds.getY(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(fillBounds, 3.0f);

    g.setColour(juce::Colour(LnF::kObsidian).withAlpha(0.3f));
    for (float y = bounds.getY(); y < bounds.getBottom(); y += 4.0f)
        g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());

    g.setColour(juce::Colour(LnF::kBorder));
    g.drawRoundedRectangle(bounds, 3.0f, 0.5f);
}

void ClonadaEditor::paint(juce::Graphics& g) {
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    // Background
    g.fillAll(juce::Colour(LnF::kObsidian));

    // Rich gradient overlay
    juce::ColourGradient bgGrad(juce::Colour(0xFF0F0F12), 0, 0,
                                 juce::Colour(0xFF080810), w, h, false);
    bgGrad.addColour(0.5, juce::Colour(0xFF0A0A0F));
    g.setGradientFill(bgGrad);
    g.fillAll();

    // ── Header bar ──
    auto headerH = 54.0f;
    juce::ColourGradient headerGrad(juce::Colour(0xFF141418), 0, 0,
                                     juce::Colour(0xFF0C0C10), 0, headerH, false);
    g.setGradientFill(headerGrad);
    g.fillRect(0.0f, 0.0f, w, headerH);
    g.setColour(juce::Colour(LnF::kCyanGlow).withAlpha(0.2f));
    g.drawLine(0, headerH - 0.5f, w, headerH - 0.5f, 1.0f);
    g.setColour(juce::Colour(LnF::kBorder));
    g.drawLine(0, headerH, w, headerH, 0.5f);

    // ── Stylized CLONΛDΛ logo with wide letter-spacing ──
    {
        auto logoFont = juce::Font(juce::FontOptions(20.0f, juce::Font::bold));
        float logoX = 18.0f;
        float logoY = 14.0f;
        float spacing = 5.0f;

        g.setFont(logoFont);

        const char* whiteChars[] = { "C", "L", "O", "N" };
        float cx = logoX;
        g.setColour(juce::Colour(LnF::kTextWhite));
        for (auto* ch : whiteChars) {
            g.drawText(ch, (int)cx, (int)logoY, 16, 26, juce::Justification::centredLeft, false);
            cx += logoFont.getStringWidthFloat(ch) + spacing;
        }

        const char* cyanChars[] = { "\xce\x9b", "D", "\xce\x9b" };
        g.setColour(juce::Colour(LnF::kCyanGlow));
        for (auto* ch : cyanChars) {
            g.drawText(juce::CharPointer_UTF8(ch), (int)cx, (int)logoY, 16, 26, juce::Justification::centredLeft, false);
            cx += logoFont.getStringWidthFloat(juce::CharPointer_UTF8(ch)) + spacing;
        }

        // Cyan underline accent bar
        float barStart = logoX + (logoFont.getStringWidthFloat("C") + spacing) * 4;
        g.setColour(juce::Colour(LnF::kCyanGlow).withAlpha(0.7f));
        g.fillRect(barStart, logoY + 27.0f, cx - barStart - spacing, 2.0f);
    }

    // Pulsing glow on status dot
    if (processor_.isEngineConnected()) {
        float pulseAlpha = 0.3f + 0.2f * std::sin(statusPulse_);
        auto dotBounds = statusDotLabel_.getBounds().toFloat().expanded(4.0f);
        g.setColour(juce::Colour(LnF::kCyanGlow).withAlpha(pulseAlpha));
        g.fillEllipse(dotBounds);
    }

    // ── Preset row ──
    auto presetY = headerH;
    g.setColour(juce::Colour(LnF::kSlatePanel).withAlpha(0.5f));
    g.fillRect(0.0f, presetY, w, 34.0f);
    g.setColour(juce::Colour(LnF::kBorder).withAlpha(0.5f));
    g.drawLine(0, presetY + 34.0f, w, presetY + 34.0f, 0.5f);

    // ── Tab bar ──
    auto tabY = presetY + 34.0f;
    g.setColour(juce::Colour(LnF::kSlatePanel).withAlpha(0.3f));
    g.fillRect(0.0f, tabY, w, 36.0f);
    g.setColour(juce::Colour(LnF::kBorder).withAlpha(0.3f));
    g.drawLine(0, tabY + 36.0f, w, tabY + 36.0f, 0.5f);

    // Active tab underline
    juce::TextButton* tabs[] = { &tabModeling_, &tabPerformance_, &tabSettings_ };
    auto& activeTabBtn = *tabs[activeTab_];
    auto tabBounds = activeTabBtn.getBounds().toFloat();
    g.setColour(juce::Colour(LnF::kCyanGlow).withAlpha(0.25f));
    g.fillRoundedRectangle(tabBounds.getX() + 4, tabBounds.getBottom() - 4.0f,
                            tabBounds.getWidth() - 8, 4.0f, 2.0f);
    g.setColour(juce::Colour(LnF::kCyanGlow));
    g.fillRoundedRectangle(tabBounds.getX() + 8, tabBounds.getBottom() - 3.0f,
                            tabBounds.getWidth() - 16, 3.0f, 1.5f);

    // ── Content panel (sharp industrial edges) ──
    auto contentY = tabY + 36.0f;
    auto contentH = h - contentY - 34.0f;
    g.setColour(juce::Colour(LnF::kSlatePanel).withAlpha(0.15f));
    g.fillRect(12.0f, contentY + 8.0f, w - 24.0f, contentH - 8.0f);
    g.setColour(juce::Colour(LnF::kCyanGlow).withAlpha(0.05f));
    g.drawRect(juce::Rectangle<float>(12.0f, contentY + 8.0f, w - 24.0f, contentH - 8.0f), 1.0f);

    // ── Performance Bridge: draw meters ──
    if (activeTab_ == 1) {
        drawMeter(g, juce::Rectangle<float>(28.0f, contentY + 30.0f, 10.0f, contentH - 70.0f),
                  inputMeter_, juce::Colour(LnF::kCyanGlow));
        drawMeter(g, juce::Rectangle<float>(w - 38.0f, contentY + 30.0f, 10.0f, contentH - 70.0f),
                  outputMeter_, juce::Colour(LnF::kCyanGlow));
    }

    // ── Modeling Studio: Apply button glow ──
    if (activeTab_ == 0 && applyButton_.isVisible()) {
        auto btnBounds = applyButton_.getBounds().toFloat();
        float glowAlpha = 0.05f + 0.03f * std::sin(statusPulse_ * 1.5f);
        g.setColour(juce::Colour(LnF::kCyanGlow).withAlpha(glowAlpha));
        g.fillRoundedRectangle(btnBounds.expanded(14.0f), 14.0f);
        float innerGlow = 0.1f + 0.05f * std::sin(statusPulse_ * 1.5f);
        g.setColour(juce::Colour(LnF::kCyanGlow).withAlpha(innerGlow));
        g.fillRoundedRectangle(btnBounds.expanded(4.0f), 8.0f);
    }

    // ── Footer bar ──
    g.setColour(juce::Colour(0xFF101014).withAlpha(0.8f));
    g.fillRect(0.0f, h - 34.0f, w, 34.0f);
    g.setColour(juce::Colour(LnF::kCyanGlow).withAlpha(0.1f));
    g.drawLine(0, h - 34.0f, w, h - 34.0f, 0.5f);

    // Footer content
    g.setColour(juce::Colour(LnF::kTextGrey));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(juce::String(processor_.getLatencySamples()) + " samples",
               14, (int)h - 28, 120, 20, juce::Justification::centredLeft);

    auto& lic = processor_.getLicenseClient();
    if (lic.isActivated()) {
        auto tierStr = lic.getTier() == LicenseClient::Tier::Advanced ? "ADVANCED" : "BASIC";
        g.setColour(juce::Colour(LnF::kCyanGlow));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(tierStr, (int)w - 120, (int)h - 28, 100, 20, juce::Justification::centredRight);
    }

    g.setColour(juce::Colour(LnF::kTextGrey));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("mediaXtreme LLC", (int)(w / 2 - 80), (int)h - 28, 160, 20, juce::Justification::centred);
}

void ClonadaEditor::resized() {
    auto area = getLocalBounds();

    // Header (0-54) — logo drawn in paint(), leave space for it
    auto header = area.removeFromTop(54).reduced(14, 0);
    header.removeFromLeft(130);
    statusDotLabel_.setBounds(header.removeFromLeft(18).withTrimmedTop(18).withHeight(18));
    statusLabel_.setBounds(header.removeFromLeft(200).withTrimmedTop(16));
    versionLabel_.setBounds(header.withTrimmedTop(18));

    // Preset row (48-82)
    auto presetRow = area.removeFromTop(34).reduced(14, 4);
    savePresetButton_.setBounds(presetRow.removeFromRight(50).withHeight(24).withTrimmedTop(1));
    presetRow.removeFromRight(6);
    presetSelector_.setBounds(presetRow.withHeight(24).withTrimmedTop(1));

    // Tab bar (82-118)
    auto tabRow = area.removeFromTop(36).reduced(14, 0);
    int tabW = tabRow.getWidth() / 3;
    tabModeling_.setBounds(tabRow.removeFromLeft(tabW).withTrimmedTop(4).withHeight(28));
    tabPerformance_.setBounds(tabRow.removeFromLeft(tabW).withTrimmedTop(4).withHeight(28));
    tabSettings_.setBounds(tabRow.withTrimmedTop(4).withHeight(28));

    // Footer removal
    area.removeFromBottom(34);

    // Content area
    auto content = area.reduced(20, 8);

    if (activeTab_ == 0) {
        // ── Modeling Studio ──
        auto modelRow = content.removeFromTop(38);
        identityBankLabel_.setBounds(modelRow.removeFromLeft(130).withTrimmedTop(6));
        browseButton_.setBounds(modelRow.removeFromRight(32).withHeight(28).withTrimmedTop(3));
        modelRow.removeFromRight(6);
        modelSelector_.setBounds(modelRow.withHeight(28).withTrimmedTop(3));

        content.removeFromTop(12);

        // 4 knobs in a row
        auto knobRow = content.removeFromTop(160);
        int knobW = knobRow.getWidth() / 4;

        auto pk = knobRow.removeFromLeft(knobW);
        pitchLabel_.setBounds(pk.removeFromTop(20));
        pitchSlider_.setBounds(pk.reduced(2, 0));

        auto mk = knobRow.removeFromLeft(knobW);
        mixLabel_.setBounds(mk.removeFromTop(20));
        mixSlider_.setBounds(mk.reduced(2, 0));

        auto fk = knobRow.removeFromLeft(knobW);
        formantLabel_.setBounds(fk.removeFromTop(20));
        formantSlider_.setBounds(fk.reduced(2, 0));

        auto gk = knobRow;
        gritLabel_.setBounds(gk.removeFromTop(20));
        gritSlider_.setBounds(gk.reduced(2, 0));

        content.removeFromTop(8);

        // Pitch tracker row
        auto trackerRow = content.removeFromTop(32);
        pitchTrackerLabel_.setBounds(trackerRow.removeFromLeft(120).withTrimmedTop(4));
        rmvpeButton_.setBounds(trackerRow.removeFromLeft(100).withTrimmedTop(2));
        crepeButton_.setBounds(trackerRow.removeFromLeft(100).withTrimmedTop(2));

        content.removeFromTop(10);

        // Mode cards
        auto modeRow = content.removeFromTop(48);
        int cardW = (modeRow.getWidth() - 12) / 2;
        lowLatencyCard_.setBounds(modeRow.removeFromLeft(cardW));
        modeRow.removeFromLeft(12);
        highQualityCard_.setBounds(modeRow.removeFromLeft(cardW));

        content.removeFromTop(14);

        // Apply button
        auto applyRow = content.removeFromTop(46);
        applyButton_.setBounds(applyRow.reduced(content.getWidth() / 6, 0));

    } else if (activeTab_ == 1) {
        // ── Performance Bridge ──
        auto left = content.removeFromLeft(52);
        inputGainLabel_.setBounds(left.removeFromTop(16));
        inputGainSlider_.setBounds(left.reduced(6, 4));

        auto right = content.removeFromRight(52);
        outputGainLabel_.setBounds(right.removeFromTop(16));
        outputGainSlider_.setBounds(right.reduced(6, 4));

        content.removeFromLeft(8);
        content.removeFromRight(8);

        auto waveArea = content.removeFromTop(content.getHeight() - 40);
        waveformDisplay_.setBounds(waveArea.reduced(0, 8));

        bypassButton_.setBounds(content.withTrimmedTop(8).removeFromLeft(100));

    } else if (activeTab_ == 2) {
        // ── Settings ──
        content.removeFromTop(14);
        licenseButton_.setBounds(content.removeFromTop(36).reduced(content.getWidth() / 4, 0));
        content.removeFromTop(16);

        // RunPod Cloud GPU section
        runpodSectionLabel_.setBounds(content.removeFromTop(24).reduced(20, 0));
        content.removeFromTop(8);
        auto rpRow = content.removeFromTop(32).reduced(20, 0);
        runpodKeyLabel_.setBounds(rpRow.removeFromLeft(70).withTrimmedTop(4));
        runpodSaveButton_.setBounds(rpRow.removeFromRight(80).withHeight(28).withTrimmedTop(1));
        rpRow.removeFromRight(6);
        runpodKeyInput_.setBounds(rpRow.withHeight(28).withTrimmedTop(1));
        content.removeFromTop(6);
        runpodStatusLabel_.setBounds(content.removeFromTop(18).reduced(74, 0));

        content.removeFromTop(16);
        engineInfoLabel_.setBounds(content.removeFromTop(56).reduced(20, 0));
        content.removeFromTop(12);
        buildInfoLabel_.setBounds(content.removeFromTop(40).reduced(20, 0));
    }

    // Overlays
    if (licensePanel_)
        licensePanel_->setBounds(getLocalBounds().reduced(160, 130));
    if (modelBrowser_)
        modelBrowser_->setBounds(getLocalBounds().reduced(40, 60));
}

juce::AudioProcessorEditor* ClonadaProcessor::createEditor() {
    return new ClonadaEditor(*this);
}
