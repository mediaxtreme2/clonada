#include "PluginEditor.h"

ClonadaEditor::ClonadaEditor(ClonadaProcessor& p)
    : AudioProcessorEditor(&p), processor_(p) {

    setSize(600, 480);
    setResizable(false, false);

    // Title
    titleLabel_.setText("CLONADA", juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(kIndigo));
    addAndMakeVisible(titleLabel_);

    // Status indicator
    statusLabel_.setText("Disconnected", juce::dontSendNotification);
    statusLabel_.setFont(juce::FontOptions(12.0f));
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(kRed));
    statusLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(statusLabel_);

    // Model selector
    modelLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    modelLabel_.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(modelLabel_);

    modelSelector_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kPanel));
    modelSelector_.setColour(juce::ComboBox::textColourId, juce::Colour(kTextLight));
    modelSelector_.setColour(juce::ComboBox::outlineColourId, juce::Colour(kIndigo).withAlpha(0.5f));
    addAndMakeVisible(modelSelector_);
    populateModelList();

    browseButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(kIndigo));
    browseButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(kTextLight));
    browseButton_.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>("Select Models Folder",
            processor_.getModelsDirectory(), "");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.isDirectory()) {
                    processor_.setModelsDirectory(result);
                    populateModelList();
                }
            });
    };
    addAndMakeVisible(browseButton_);

    // Pitch knob
    pitchSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    pitchSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    pitchSlider_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(kIndigo));
    pitchSlider_.setColour(juce::Slider::thumbColourId, juce::Colour(kCyan));
    pitchSlider_.setColour(juce::Slider::textBoxTextColourId, juce::Colour(kTextLight));
    pitchSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(pitchSlider_);
    pitchLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    pitchLabel_.setJustificationType(juce::Justification::centred);
    pitchLabel_.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(pitchLabel_);

    // Formant knob
    formantSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    formantSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    formantSlider_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(kIndigo));
    formantSlider_.setColour(juce::Slider::thumbColourId, juce::Colour(kCyan));
    formantSlider_.setColour(juce::Slider::textBoxTextColourId, juce::Colour(kTextLight));
    formantSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(formantSlider_);
    formantLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    formantLabel_.setJustificationType(juce::Justification::centred);
    formantLabel_.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(formantLabel_);

    // Mix knob
    mixSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    mixSlider_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(kCyan));
    mixSlider_.setColour(juce::Slider::thumbColourId, juce::Colour(kIndigo));
    mixSlider_.setColour(juce::Slider::textBoxTextColourId, juce::Colour(kTextLight));
    mixSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(mixSlider_);
    mixLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    mixLabel_.setJustificationType(juce::Justification::centred);
    mixLabel_.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(mixLabel_);

    // Input gain slider (vertical)
    inputGainSlider_.setSliderStyle(juce::Slider::LinearVertical);
    inputGainSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    inputGainSlider_.setColour(juce::Slider::trackColourId, juce::Colour(kIndigo));
    inputGainSlider_.setColour(juce::Slider::textBoxTextColourId, juce::Colour(kTextLight));
    inputGainSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(inputGainSlider_);
    inputGainLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    inputGainLabel_.setJustificationType(juce::Justification::centred);
    inputGainLabel_.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(inputGainLabel_);

    // Output gain slider (vertical)
    outputGainSlider_.setSliderStyle(juce::Slider::LinearVertical);
    outputGainSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    outputGainSlider_.setColour(juce::Slider::trackColourId, juce::Colour(kCyan));
    outputGainSlider_.setColour(juce::Slider::textBoxTextColourId, juce::Colour(kTextLight));
    outputGainSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(outputGainSlider_);
    outputGainLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    outputGainLabel_.setJustificationType(juce::Justification::centred);
    outputGainLabel_.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(outputGainLabel_);

    // Mode selector
    modeLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    modeLabel_.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(modeLabel_);

    modeSelector_.addItem("Low Latency", 1);
    modeSelector_.addItem("High Quality", 2);
    modeSelector_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kPanel));
    modeSelector_.setColour(juce::ComboBox::textColourId, juce::Colour(kTextLight));
    modeSelector_.setColour(juce::ComboBox::outlineColourId, juce::Colour(kIndigo).withAlpha(0.5f));
    addAndMakeVisible(modeSelector_);

    // Bypass
    bypassButton_.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextLight));
    bypassButton_.setColour(juce::ToggleButton::tickColourId, juce::Colour(kRed));
    addAndMakeVisible(bypassButton_);

    // Attachments
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
}

void ClonadaEditor::timerCallback() {
    updateConnectionStatus();

    inputMeter_ = processor_.getCurrentInputLevel();
    outputMeter_ = processor_.getCurrentOutputLevel();
    repaint();
}

void ClonadaEditor::updateConnectionStatus() {
    if (processor_.isEngineConnected()) {
        statusLabel_.setText("Engine Connected", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(kGreen));
    } else {
        statusLabel_.setText("Engine Disconnected", juce::dontSendNotification);
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

void ClonadaEditor::paint(juce::Graphics& g) {
    // Background
    g.fillAll(juce::Colour(kBgDark));

    // Header bar
    g.setColour(juce::Colour(kPanel));
    g.fillRoundedRectangle(10.0f, 10.0f, getWidth() - 20.0f, 50.0f, 8.0f);

    // Controls panel
    g.setColour(juce::Colour(kPanel));
    g.fillRoundedRectangle(10.0f, 120.0f, getWidth() - 20.0f, 240.0f, 8.0f);

    // Bottom panel (mode/bypass)
    g.fillRoundedRectangle(10.0f, 370.0f, getWidth() - 20.0f, 50.0f, 8.0f);

    // Input meter
    auto meterX = 22.0f;
    auto meterY = 130.0f;
    auto meterW = 8.0f;
    auto meterH = 220.0f;
    g.setColour(juce::Colour(0xFF1e293b));
    g.fillRoundedRectangle(meterX, meterY, meterW, meterH, 3.0f);
    float inH = juce::jlimit(0.0f, 1.0f, inputMeter_) * meterH;
    g.setColour(juce::Colour(kGreen));
    g.fillRoundedRectangle(meterX, meterY + meterH - inH, meterW, inH, 3.0f);

    // Output meter
    meterX = getWidth() - 30.0f;
    g.setColour(juce::Colour(0xFF1e293b));
    g.fillRoundedRectangle(meterX, meterY, meterW, meterH, 3.0f);
    float outH = juce::jlimit(0.0f, 1.0f, outputMeter_) * meterH;
    g.setColour(juce::Colour(kCyan));
    g.fillRoundedRectangle(meterX, meterY + meterH - outH, meterW, outH, 3.0f);

    // Latency display
    g.setColour(juce::Colour(kTextDim));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(juce::String(processor_.getLatencySamples()) + " samples latency",
               10, getHeight() - 25, 200, 20, juce::Justification::centredLeft);

    // Version
    g.drawText("v1.0.0", getWidth() - 80, getHeight() - 25, 70, 20,
               juce::Justification::centredRight);
}

void ClonadaEditor::resized() {
    auto area = getLocalBounds();

    // Header
    auto header = area.removeFromTop(60).reduced(10);
    titleLabel_.setBounds(header.removeFromLeft(180).withTrimmedTop(10));
    statusLabel_.setBounds(header.withTrimmedTop(15));

    // Model row
    auto modelRow = area.removeFromTop(55).reduced(20, 5);
    modelLabel_.setBounds(modelRow.removeFromLeft(90).withTrimmedTop(12));
    browseButton_.setBounds(modelRow.removeFromRight(35).withTrimmedTop(8).withHeight(28));
    modelSelector_.setBounds(modelRow.withTrimmedTop(8).withHeight(28));

    // Controls area
    auto controls = area.removeFromTop(250).reduced(40, 10);

    // Input gain fader on left
    auto leftFader = controls.removeFromLeft(50);
    inputGainLabel_.setBounds(leftFader.removeFromTop(16));
    inputGainSlider_.setBounds(leftFader.reduced(5, 0));

    // Output gain fader on right
    auto rightFader = controls.removeFromRight(50);
    outputGainLabel_.setBounds(rightFader.removeFromTop(16));
    outputGainSlider_.setBounds(rightFader.reduced(5, 0));

    // Three knobs in center
    auto knobArea = controls.reduced(20, 10);
    int knobW = knobArea.getWidth() / 3;

    auto pitchArea = knobArea.removeFromLeft(knobW);
    pitchLabel_.setBounds(pitchArea.removeFromTop(16));
    pitchSlider_.setBounds(pitchArea.reduced(5));

    auto formantArea = knobArea.removeFromLeft(knobW);
    formantLabel_.setBounds(formantArea.removeFromTop(16));
    formantSlider_.setBounds(formantArea.reduced(5));

    auto mixArea = knobArea;
    mixLabel_.setBounds(mixArea.removeFromTop(16));
    mixSlider_.setBounds(mixArea.reduced(5));

    // Bottom bar
    auto bottom = area.removeFromTop(55).reduced(20, 10);
    modeLabel_.setBounds(bottom.removeFromLeft(45).withTrimmedTop(8));
    modeSelector_.setBounds(bottom.removeFromLeft(140).withTrimmedTop(6).withHeight(28));
    bottom.removeFromLeft(20);
    bypassButton_.setBounds(bottom.removeFromLeft(100).withTrimmedTop(6));
}

juce::AudioProcessorEditor* ClonadaProcessor::createEditor() {
    return new ClonadaEditor(*this);
}
