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

    static constexpr juce::uint32 kBgDark       = 0xFF08080a;
    static constexpr juce::uint32 kBgMid        = 0xFF0e0e14;
    static constexpr juce::uint32 kPanel         = 0xFF161626;
    static constexpr juce::uint32 kPanelLight    = 0xFF1e1e36;
    static constexpr juce::uint32 kIndigo        = 0xFF6366f1;
    static constexpr juce::uint32 kIndigoGlow    = 0xFF818cf8;
    static constexpr juce::uint32 kCyan          = 0xFF06b6d4;
    static constexpr juce::uint32 kCyanGlow      = 0xFF22d3ee;
    static constexpr juce::uint32 kTextLight     = 0xFFe2e8f0;
    static constexpr juce::uint32 kTextDim       = 0xFF64748b;
    static constexpr juce::uint32 kTextMuted     = 0xFF475569;
    static constexpr juce::uint32 kGreen         = 0xFF22c55e;
    static constexpr juce::uint32 kRed           = 0xFFef4444;
    static constexpr juce::uint32 kAmber         = 0xFFf59e0b;
    static constexpr juce::uint32 kKnobTrack     = 0xFF1e293b;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClonadaLookAndFeel)
};
