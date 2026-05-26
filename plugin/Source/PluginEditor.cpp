#include "PluginEditor.h"

static constexpr juce::uint32 kBgDark    = 0xFF08080a;
static constexpr juce::uint32 kBgMid     = 0xFF0e0e14;
static constexpr juce::uint32 kPanel     = 0xFF161626;
static constexpr juce::uint32 kPanelLt   = 0xFF1e1e36;
static constexpr juce::uint32 kIndigo    = 0xFF6366f1;
static constexpr juce::uint32 kIndigoGl  = 0xFF818cf8;
static constexpr juce::uint32 kCyan      = 0xFF06b6d4;
static constexpr juce::uint32 kCyanGl    = 0xFF22d3ee;
static constexpr juce::uint32 kTextLight = 0xFFe2e8f0;
static constexpr juce::uint32 kTextDim   = 0xFF64748b;
static constexpr juce::uint32 kGreen     = 0xFF22c55e;
static constexpr juce::uint32 kRed       = 0xFFef4444;

ClonadaEditor::ClonadaEditor(ClonadaProcessor& p)
    : AudioProcessorEditor(&p), processor_(p),
      presetManager_(p.getAPVTS()),
      waveformDisplay_(p.getWaveformDisplay()) {

    setLookAndFeel(&lnf_);
    setSize(720, 560);
    setResizable(false, false);

    // Title
    titleLabel_.setText("CLONADA", juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(kIndigo));
    addAndMakeVisible(titleLabel_);

    versionLabel_.setText("v1.0.0", juce::dontSendNotification);
    versionLabel_.setFont(juce::FontOptions(10.0f));
    versionLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    addAndMakeVisible(versionLabel_);

    // Status
    statusLabel_.setText("Disconnected", juce::dontSendNotification);
    statusLabel_.setFont(juce::FontOptions(11.0f));
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(kRed));
    statusLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(statusLabel_);

    // Preset selector
    presetSelector_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kPanel));
    presetSelector_.setColour(juce::ComboBox::textColourId, juce::Colour(kTextLight));
    presetSelector_.setColour(juce::ComboBox::outlineColourId, juce::Colour(kIndigo).withAlpha(0.3f));
    presetSelector_.onChange = [this] {
        presetManager_.loadPreset(presetSelector_.getSelectedId() - 1);
    };
    addAndMakeVisible(presetSelector_);
    populatePresetList();

    savePresetButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(kPanel));
    savePresetButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(kTextDim));
    savePresetButton_.onClick = [this] {
        auto name = juce::String("User Preset ") + juce::String(presetManager_.getNumPresets() + 1);
        presetManager_.savePreset(name);
        populatePresetList();
    };
    addAndMakeVisible(savePresetButton_);

    // Model selector
    modelLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    modelLabel_.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    addAndMakeVisible(modelLabel_);

    modelSelector_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kPanel));
    modelSelector_.setColour(juce::ComboBox::textColourId, juce::Colour(kTextLight));
    modelSelector_.setColour(juce::ComboBox::outlineColourId, juce::Colour(kCyan).withAlpha(0.3f));
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

    browseButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(kCyan).withAlpha(0.2f));
    browseButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(kCyan));
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

    // Knob setup helper
    auto setupKnob = [this](juce::Slider& s, juce::Colour fill, juce::Colour thumb) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 16);
        s.setColour(juce::Slider::rotarySliderFillColourId, fill);
        s.setColour(juce::Slider::thumbColourId, thumb);
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(kTextLight));
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(s);
    };

    auto setupLabel = [this](juce::Label& l) {
        l.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        addAndMakeVisible(l);
    };

    setupKnob(pitchSlider_, juce::Colour(kIndigo), juce::Colour(kIndigoGl));
    setupKnob(formantSlider_, juce::Colour(kIndigo), juce::Colour(kIndigoGl));
    setupKnob(mixSlider_, juce::Colour(kCyan), juce::Colour(kCyanGl));
    setupLabel(pitchLabel_);
    setupLabel(formantLabel_);
    setupLabel(mixLabel_);

    // Gain faders
    auto setupFader = [this](juce::Slider& s, juce::Colour track) {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 14);
        s.setColour(juce::Slider::trackColourId, track);
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(kTextLight));
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(s);
    };

    setupFader(inputGainSlider_, juce::Colour(kIndigo));
    setupFader(outputGainSlider_, juce::Colour(kCyan));
    setupLabel(inputGainLabel_);
    setupLabel(outputGainLabel_);

    // Mode selector
    modeLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    modeLabel_.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    addAndMakeVisible(modeLabel_);

    modeSelector_.addItem("Low Latency", 1);
    modeSelector_.addItem("High Quality", 2);
    modeSelector_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kPanel));
    modeSelector_.setColour(juce::ComboBox::textColourId, juce::Colour(kTextLight));
    modeSelector_.setColour(juce::ComboBox::outlineColourId, juce::Colour(kIndigo).withAlpha(0.3f));
    addAndMakeVisible(modeSelector_);

    // Bypass
    bypassButton_.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextLight));
    bypassButton_.setColour(juce::ToggleButton::tickColourId, juce::Colour(kRed));
    addAndMakeVisible(bypassButton_);

    // License button
    licenseButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(kPanel));
    licenseButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(kCyan));
    licenseButton_.onClick = [this] {
        showingLicense_ = !showingLicense_;
        if (showingLicense_) {
            licensePanel_ = std::make_unique<LicensePanel>(processor_.getLicenseClient());
            licensePanel_->onLicenseActivated = [this] { processor_.launchEngine(); };
            addAndMakeVisible(*licensePanel_);
            licensePanel_->setBounds(getLocalBounds().reduced(160, 130));
        } else {
            licensePanel_.reset();
        }
        repaint();
    };
    addAndMakeVisible(licenseButton_);

    // Waveform
    addAndMakeVisible(waveformDisplay_);

    // APVTS Attachments
    pitchAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::PITCH, pitchSlider_);
    formantAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::FORMANT, formantSlider_);
    mixAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::MIX, mixSlider_);
    inputGainAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::INPUT_GAIN, inputGainSlider_);
    outputGainAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.getAPVTS(), ParamIDs::OUTPUT_GAIN, outputGainSlider_);
    modeAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.getAPVTS(), ParamIDs::MODE, modeSelector_);
    bypassAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.getAPVTS(), ParamIDs::BYPASS, bypassButton_);

    startTimerHz(30);
}

ClonadaEditor::~ClonadaEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void ClonadaEditor::timerCallback() {
    updateConnectionStatus();
    inputMeter_ = processor_.getCurrentInputLevel();
    outputMeter_ = processor_.getCurrentOutputLevel();
    repaint();
}

void ClonadaEditor::updateConnectionStatus() {
    auto& bridge = processor_.getBridge();
    if (processor_.isEngineConnected()) {
        if (bridge.isModelLoaded()) {
            statusLabel_.setText("Voice: " + bridge.getLoadedModelName(), juce::dontSendNotification);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(kCyan));
        } else {
            statusLabel_.setText("Engine Ready - Select Model", juce::dontSendNotification);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(kGreen));
        }
    } else {
        statusLabel_.setText("Engine Offline", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(kRed));
    }
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
    // Background
    g.setColour(juce::Colour(0xFF0c0c14));
    g.fillRoundedRectangle(bounds, 3.0f);

    float clampedLevel = juce::jlimit(0.0f, 1.0f, level);
    float fillH = clampedLevel * bounds.getHeight();

    // Glow
    auto fillBounds = bounds.withTop(bounds.getBottom() - fillH);
    g.setColour(colour.withAlpha(0.1f));
    g.fillRoundedRectangle(fillBounds.expanded(2.0f, 0.0f), 3.0f);

    // Fill with gradient
    juce::ColourGradient grad(colour, bounds.getCentreX(), bounds.getBottom(),
                               colour.brighter(0.3f), bounds.getCentreX(), fillBounds.getY(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(fillBounds, 3.0f);

    // Segmented look
    g.setColour(juce::Colour(kBgDark).withAlpha(0.4f));
    for (float y = bounds.getY(); y < bounds.getBottom(); y += 4.0f)
        g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());

    // Border
    g.setColour(juce::Colour(kPanelLt));
    g.drawRoundedRectangle(bounds, 3.0f, 0.5f);
}

void ClonadaEditor::paint(juce::Graphics& g) {
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    // Background gradient
    juce::ColourGradient bgGrad(juce::Colour(kBgDark), 0, 0,
                                 juce::Colour(kBgMid), w, h, false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // Header bar
    auto headerBounds = juce::Rectangle<float>(0, 0, w, 52.0f);
    g.setColour(juce::Colour(kPanel).withAlpha(0.8f));
    g.fillRect(headerBounds);
    g.setColour(juce::Colour(kIndigo).withAlpha(0.15f));
    g.drawLine(0, 52.0f, w, 52.0f, 1.0f);

    // Preset row
    g.setColour(juce::Colour(kPanel).withAlpha(0.5f));
    g.fillRect(0.0f, 52.0f, w, 36.0f);
    g.setColour(juce::Colour(kIndigo).withAlpha(0.08f));
    g.drawLine(0, 88.0f, w, 88.0f, 0.5f);

    // Model row
    g.setColour(juce::Colour(kPanel).withAlpha(0.3f));
    g.fillRect(0.0f, 88.0f, w, 36.0f);
    g.setColour(juce::Colour(kIndigo).withAlpha(0.08f));
    g.drawLine(0, 124.0f, w, 124.0f, 0.5f);

    // Controls panel
    g.setColour(juce::Colour(kPanel).withAlpha(0.4f));
    g.fillRoundedRectangle(14.0f, 130.0f, w - 28.0f, 220.0f, 10.0f);
    g.setColour(juce::Colour(kIndigo).withAlpha(0.1f));
    g.drawRoundedRectangle(14.0f, 130.0f, w - 28.0f, 220.0f, 10.0f, 0.5f);

    // Input meter (left of controls)
    drawMeter(g, juce::Rectangle<float>(24.0f, 145.0f, 10.0f, 190.0f), inputMeter_, juce::Colour(kIndigo));

    // Output meter (right of controls)
    drawMeter(g, juce::Rectangle<float>(w - 34.0f, 145.0f, 10.0f, 190.0f), outputMeter_, juce::Colour(kCyan));

    // Bottom bar
    g.setColour(juce::Colour(kPanel).withAlpha(0.6f));
    g.fillRect(0.0f, h - 36.0f, w, 36.0f);
    g.setColour(juce::Colour(kIndigo).withAlpha(0.1f));
    g.drawLine(0, h - 36.0f, w, h - 36.0f, 0.5f);

    // Latency display
    g.setColour(juce::Colour(kTextDim));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText(juce::String(processor_.getLatencySamples()) + " samples",
               14, (int)h - 30, 120, 20, juce::Justification::centredLeft);

    // License tier indicator
    auto& lic = processor_.getLicenseClient();
    if (lic.isActivated()) {
        auto tierStr = lic.getTier() == LicenseClient::Tier::Advanced ? "ADVANCED" : "BASIC";
        g.setColour(juce::Colour(kGreen).withAlpha(0.7f));
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(tierStr, (int)w - 134, (int)h - 30, 60, 20, juce::Justification::centredRight);
    }
}

void ClonadaEditor::resized() {
    auto area = getLocalBounds();

    // Header (0-52)
    auto header = area.removeFromTop(52).reduced(14, 0);
    titleLabel_.setBounds(header.removeFromLeft(140).withTrimmedTop(12));
    versionLabel_.setBounds(header.removeFromLeft(50).withTrimmedTop(20));
    licenseButton_.setBounds(header.removeFromRight(65).withTrimmedTop(14).withHeight(24));
    statusLabel_.setBounds(header.withTrimmedTop(16));

    // Preset row (52-88)
    auto presetRow = area.removeFromTop(36).reduced(14, 4);
    savePresetButton_.setBounds(presetRow.removeFromRight(50).withHeight(26).withTrimmedTop(1));
    presetRow.removeFromRight(6);
    presetSelector_.setBounds(presetRow.withHeight(26).withTrimmedTop(1));

    // Model row (88-124)
    auto modelRow = area.removeFromTop(36).reduced(14, 4);
    modelLabel_.setBounds(modelRow.removeFromLeft(90).withTrimmedTop(4));
    browseButton_.setBounds(modelRow.removeFromRight(32).withHeight(26).withTrimmedTop(1));
    modelRow.removeFromRight(6);
    modelSelector_.setBounds(modelRow.withHeight(26).withTrimmedTop(1));

    // Controls area (130-350) - inside the rounded panel
    auto controls = area.removeFromTop(226).reduced(44, 10);

    // Input gain fader on left
    auto leftFader = controls.removeFromLeft(44);
    inputGainLabel_.setBounds(leftFader.removeFromTop(14));
    inputGainSlider_.setBounds(leftFader.reduced(4, 0));

    // Output gain fader on right
    auto rightFader = controls.removeFromRight(44);
    outputGainLabel_.setBounds(rightFader.removeFromTop(14));
    outputGainSlider_.setBounds(rightFader.reduced(4, 0));

    // Three knobs in center
    auto knobArea = controls.reduced(20, 4);
    int knobW = knobArea.getWidth() / 3;

    auto pitchArea = knobArea.removeFromLeft(knobW);
    pitchLabel_.setBounds(pitchArea.removeFromTop(14));
    pitchSlider_.setBounds(pitchArea.reduced(4));

    auto formantArea = knobArea.removeFromLeft(knobW);
    formantLabel_.setBounds(formantArea.removeFromTop(14));
    formantSlider_.setBounds(formantArea.reduced(4));

    auto mixArea = knobArea;
    mixLabel_.setBounds(mixArea.removeFromTop(14));
    mixSlider_.setBounds(mixArea.reduced(4));

    // Waveform display
    auto waveArea = area.removeFromTop(90).reduced(14, 6);
    waveformDisplay_.setBounds(waveArea);

    // Bottom bar
    auto bottom = area.reduced(14, 0).withTrimmedTop(6);
    modeLabel_.setBounds(bottom.removeFromLeft(40).withTrimmedTop(6));
    modeSelector_.setBounds(bottom.removeFromLeft(130).withTrimmedTop(4).withHeight(26));
    bottom.removeFromLeft(16);
    bypassButton_.setBounds(bottom.removeFromLeft(90).withTrimmedTop(4));

    // License panel overlay
    if (licensePanel_)
        licensePanel_->setBounds(getLocalBounds().reduced(160, 130));

    // Model browser overlay
    if (modelBrowser_)
        modelBrowser_->setBounds(getLocalBounds().reduced(40, 60));
}

juce::AudioProcessorEditor* ClonadaProcessor::createEditor() {
    return new ClonadaEditor(*this);
}
