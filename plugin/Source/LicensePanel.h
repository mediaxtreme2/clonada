#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LicenseClient.h"

class LicensePanel : public juce::Component {
public:
    LicensePanel(LicenseClient& client);
    ~LicensePanel() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void()> onLicenseActivated;

private:
    void updateUI();
    void attemptActivation();
    void attemptDeactivation();

    LicenseClient& client_;

    juce::Label titleLabel_;
    juce::Label statusLabel_;
    juce::Label tierLabel_;
    juce::Label keyLabel_;
    juce::TextEditor keyInput_;
    juce::TextButton activateButton_{"Activate"};
    juce::TextButton deactivateButton_{"Deactivate"};
    juce::Label messageLabel_;
    bool waiting_ = false;

    static constexpr juce::uint32 kBgDark = 0xFF0a0a0c;
    static constexpr juce::uint32 kPanel = 0xFF1a1a2e;
    static constexpr juce::uint32 kIndigo = 0xFF6366f1;
    static constexpr juce::uint32 kCyan = 0xFF06b6d4;
    static constexpr juce::uint32 kTextLight = 0xFFe2e8f0;
    static constexpr juce::uint32 kTextDim = 0xFF94a3b8;
    static constexpr juce::uint32 kGreen = 0xFF22c55e;
    static constexpr juce::uint32 kRed = 0xFFef4444;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicensePanel)
};
