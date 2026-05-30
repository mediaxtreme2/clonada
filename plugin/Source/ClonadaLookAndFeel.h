#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class ClonadaLookAndFeel : public juce::LookAndFeel_V4 {
public:
    ClonadaLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& bg, bool highlighted, bool down) override;

    void drawComboBox(juce::Graphics&, int w, int h, bool down,
                      int bx, int by, int bw, int bh, juce::ComboBox&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool highlighted, bool down) override;

    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    // Obsidian + Cyan color palette
    static constexpr juce::uint32 kObsidian     = 0xFF0E0E0E;
    static constexpr juce::uint32 kSlatePanel    = 0xFF161616;
    static constexpr juce::uint32 kSlateLighter  = 0xFF1C1C1C;
    static constexpr juce::uint32 kBorder        = 0xFF262626;
    static constexpr juce::uint32 kCyanGlow      = 0xFF00F2FF;
    static constexpr juce::uint32 kCyanDim       = 0xFF00B8C4;
    static constexpr juce::uint32 kCyanSubtle    = 0xFF005F66;
    static constexpr juce::uint32 kTextWhite     = 0xFFFFFFFF;
    static constexpr juce::uint32 kTextGrey      = 0xFF888888;
    static constexpr juce::uint32 kTextDark      = 0xFF555555;
    static constexpr juce::uint32 kGreen         = 0xFF22c55e;
    static constexpr juce::uint32 kRed           = 0xFFef4444;
    static constexpr juce::uint32 kAmber         = 0xFFf59e0b;
    static constexpr juce::uint32 kKnobTrack     = 0xFF2A2A2A;

    // Legacy aliases for components that reference old names
    static constexpr juce::uint32 kBgDark    = kObsidian;
    static constexpr juce::uint32 kPanel     = kSlatePanel;
    static constexpr juce::uint32 kTextLight = kTextWhite;
    static constexpr juce::uint32 kTextDim   = kTextGrey;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClonadaLookAndFeel)
};
