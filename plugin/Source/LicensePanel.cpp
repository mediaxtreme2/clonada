#include "LicensePanel.h"

LicensePanel::LicensePanel(LicenseClient& client) : client_(client) {
    setSize(400, 300);

    titleLabel_.setText(juce::CharPointer_UTF8("CLON\xce\x9bD\xce\x9b LICENSE"), juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(kCyan));
    titleLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel_);

    statusLabel_.setFont(juce::FontOptions(13.0f));
    statusLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel_);

    tierLabel_.setFont(juce::FontOptions(12.0f));
    tierLabel_.setColour(juce::Label::textColourId, juce::Colour(kCyan));
    tierLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(tierLabel_);

    keyLabel_.setText("License Key:", juce::dontSendNotification);
    keyLabel_.setFont(juce::FontOptions(12.0f));
    keyLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    addAndMakeVisible(keyLabel_);

    keyInput_.setFont(juce::FontOptions(14.0f));
    keyInput_.setTextToShowWhenEmpty("CLON-XXXX-XXXX-XXXX-XXXX", juce::Colour(kTextDim));
    keyInput_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(kPanel));
    keyInput_.setColour(juce::TextEditor::textColourId, juce::Colour(kTextLight));
    keyInput_.setColour(juce::TextEditor::outlineColourId, juce::Colour(kCyan).withAlpha(0.3f));
    keyInput_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(kCyan));
    keyInput_.setJustification(juce::Justification::centred);
    addAndMakeVisible(keyInput_);

    activateButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(kCyan));
    activateButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(kBgDark));
    activateButton_.onClick = [this] { attemptActivation(); };
    addAndMakeVisible(activateButton_);

    deactivateButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(kRed).withAlpha(0.7f));
    deactivateButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(kTextLight));
    deactivateButton_.onClick = [this] { attemptDeactivation(); };
    addAndMakeVisible(deactivateButton_);

    messageLabel_.setFont(juce::FontOptions(11.0f));
    messageLabel_.setJustificationType(juce::Justification::centred);
    messageLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
    addAndMakeVisible(messageLabel_);

    updateUI();
}

void LicensePanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(kBgDark));
    g.setColour(juce::Colour(kPanel));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f), 12.0f);
    g.setColour(juce::Colour(kCyan).withAlpha(0.2f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f), 12.0f, 1.5f);
}

void LicensePanel::resized() {
    auto area = getLocalBounds().reduced(30);

    titleLabel_.setBounds(area.removeFromTop(35));
    area.removeFromTop(8);
    statusLabel_.setBounds(area.removeFromTop(22));
    tierLabel_.setBounds(area.removeFromTop(20));
    area.removeFromTop(15);

    keyLabel_.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);
    keyInput_.setBounds(area.removeFromTop(32).reduced(20, 0));
    area.removeFromTop(15);

    auto buttonRow = area.removeFromTop(36);
    auto halfW = buttonRow.getWidth() / 2 - 10;
    activateButton_.setBounds(buttonRow.removeFromLeft(halfW).reduced(2, 0));
    buttonRow.removeFromLeft(20);
    deactivateButton_.setBounds(buttonRow.removeFromLeft(halfW).reduced(2, 0));

    area.removeFromTop(10);
    messageLabel_.setBounds(area.removeFromTop(40));
}

void LicensePanel::updateUI() {
    auto info = client_.getCurrentInfo();
    bool activated = client_.isActivated();

    if (activated) {
        statusLabel_.setText("ACTIVATED", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(kGreen));
        auto tierStr = info.tier == LicenseClient::Tier::Advanced ? "Advanced" : "Basic";
        tierLabel_.setText(juce::String("Tier: ") + tierStr, juce::dontSendNotification);
        keyInput_.setText(info.licenseKey);
        keyInput_.setEnabled(false);
        activateButton_.setEnabled(false);
        deactivateButton_.setEnabled(true);
    } else {
        statusLabel_.setText("NOT ACTIVATED", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(kRed));
        tierLabel_.setText("", juce::dontSendNotification);
        keyInput_.setEnabled(true);
        activateButton_.setEnabled(!waiting_);
        deactivateButton_.setEnabled(false);
    }
}

void LicensePanel::attemptActivation() {
    auto key = keyInput_.getText().trim().toUpperCase();
    if (key.isEmpty()) {
        messageLabel_.setText("Please enter a license key", juce::dontSendNotification);
        messageLabel_.setColour(juce::Label::textColourId, juce::Colour(kRed));
        return;
    }

    waiting_ = true;
    activateButton_.setEnabled(false);
    messageLabel_.setText("Activating...", juce::dontSendNotification);
    messageLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));

    client_.activate(key, [this](LicenseClient::LicenseInfo info) {
        waiting_ = false;
        if (info.status == LicenseClient::Status::Valid) {
            messageLabel_.setText("License activated successfully!", juce::dontSendNotification);
            messageLabel_.setColour(juce::Label::textColourId, juce::Colour(kGreen));
            if (onLicenseActivated) onLicenseActivated();
        } else if (info.status == LicenseClient::Status::NetworkError) {
            messageLabel_.setText("Network error - check your connection", juce::dontSendNotification);
            messageLabel_.setColour(juce::Label::textColourId, juce::Colour(kRed));
        } else {
            messageLabel_.setText(info.message.isEmpty() ? "Invalid license key" : info.message, juce::dontSendNotification);
            messageLabel_.setColour(juce::Label::textColourId, juce::Colour(kRed));
        }
        updateUI();
    });
}

void LicensePanel::attemptDeactivation() {
    waiting_ = true;
    deactivateButton_.setEnabled(false);
    messageLabel_.setText("Deactivating...", juce::dontSendNotification);
    messageLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));

    client_.deactivate([this](bool success) {
        waiting_ = false;
        if (success) {
            messageLabel_.setText("License deactivated", juce::dontSendNotification);
            messageLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDim));
            keyInput_.clear();
        } else {
            messageLabel_.setText("Failed to deactivate", juce::dontSendNotification);
            messageLabel_.setColour(juce::Label::textColourId, juce::Colour(kRed));
        }
        updateUI();
    });
}
