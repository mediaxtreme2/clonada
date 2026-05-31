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
    std::function<void()> onClose;

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
    juce::TextButton closeButton_{juce::CharPointer_UTF8("\xe2\x9c\x95")};
    juce::Label messageLabel_;
    bool waiting_ = false;

    static constexpr juce::uint32 kBgDark   = 0xFF0E0E0E;
    static constexpr juce::uint32 kPanel     = 0xFF161616;
    static constexpr juce::uint32 kCyan      = 0xFF00F2FF;
    static constexpr juce::uint32 kBorder    = 0xFF262626;
    static constexpr juce::uint32 kTextLight = 0xFFFFFFFF;
    static constexpr juce::uint32 kTextDim   = 0xFF888888;
    static constexpr juce::uint32 kGreen     = 0xFF22c55e;
    static constexpr juce::uint32 kRed       = 0xFFef4444;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicensePanel)
};
