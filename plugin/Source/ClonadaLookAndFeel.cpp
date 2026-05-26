#include "ClonadaLookAndFeel.h"

ClonadaLookAndFeel::ClonadaLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(kBgDark));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(kPanel));
    setColour(juce::PopupMenu::textColourId, juce::Colour(kTextLight));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(kIndigo));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(kTextLight));
}

void ClonadaLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                           float sliderPos, float startAngle, float endAngle,
                                           juce::Slider& slider) {
    auto bounds = juce::Rectangle<float>(x, y, w, h).reduced(4.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = startAngle + sliderPos * (endAngle - startAngle);

    // Outer shadow
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillEllipse(rx + 2.0f, ry + 2.0f, rw, rw);

    // Knob body - dark gradient
    juce::ColourGradient bodyGrad(juce::Colour(0xFF2a2a3e), centreX, ry,
                                   juce::Colour(0xFF14141e), centreX, ry + rw, false);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(rx, ry, rw, rw);

    // Subtle rim highlight
    g.setColour(juce::Colour(0xFF3a3a50));
    g.drawEllipse(rx, ry, rw, rw, 1.0f);

    // Track arc (background)
    auto trackRadius = radius - 3.0f;
    juce::Path trackArc;
    trackArc.addCentredArc(centreX, centreY, trackRadius, trackRadius,
                           0.0f, startAngle, endAngle, true);
    g.setColour(juce::Colour(kKnobTrack));
    g.strokePath(trackArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // Value arc (colored)
    if (sliderPos > 0.0f) {
        juce::Path valueArc;

        auto fillColour = slider.findColour(juce::Slider::rotarySliderFillColourId);

        // For bipolar knobs (pitch/formant), draw from center
        bool bipolar = slider.getMinimum() < 0 && slider.getMaximum() > 0;
        float arcStart = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        float arcEnd = angle;
        if (bipolar && arcEnd < arcStart) std::swap(arcStart, arcEnd);

        valueArc.addCentredArc(centreX, centreY, trackRadius, trackRadius,
                               0.0f, arcStart, arcEnd, true);

        // Glow effect
        g.setColour(fillColour.withAlpha(0.15f));
        g.strokePath(valueArc, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        g.setColour(fillColour);
        g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // Pointer line
    juce::Path pointer;
    auto pointerLength = radius * 0.55f;
    auto pointerThickness = 2.5f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -radius + 6.0f,
                                 pointerThickness, pointerLength, 1.0f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

    auto thumbCol = slider.findColour(juce::Slider::thumbColourId);
    g.setColour(thumbCol);
    g.fillPath(pointer);

    // Center dot
    g.setColour(juce::Colour(0xFF3a3a50));
    g.fillEllipse(centreX - 3.0f, centreY - 3.0f, 6.0f, 6.0f);
}

void ClonadaLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                                           float sliderPos, float minSliderPos, float maxSliderPos,
                                           juce::Slider::SliderStyle style, juce::Slider& slider) {
    if (style == juce::Slider::LinearVertical) {
        auto trackWidth = 4.0f;
        auto centreX = (float)x + (float)w * 0.5f;

        // Track background
        juce::Rectangle<float> track(centreX - trackWidth * 0.5f, (float)y,
                                      trackWidth, (float)h);
        g.setColour(juce::Colour(kKnobTrack));
        g.fillRoundedRectangle(track, 2.0f);

        // Filled portion
        auto fillTop = sliderPos;
        auto fillBottom = (float)(y + h);
        auto fillColour = slider.findColour(juce::Slider::trackColourId);

        // Glow
        juce::Rectangle<float> fill(centreX - trackWidth, fillTop,
                                     trackWidth * 2.0f, fillBottom - fillTop);
        g.setColour(fillColour.withAlpha(0.12f));
        g.fillRoundedRectangle(fill, 3.0f);

        juce::Rectangle<float> fillCore(centreX - trackWidth * 0.5f, fillTop,
                                          trackWidth, fillBottom - fillTop);
        g.setColour(fillColour);
        g.fillRoundedRectangle(fillCore, 2.0f);

        // Thumb
        auto thumbSize = 12.0f;
        g.setColour(juce::Colour(kTextLight));
        g.fillRoundedRectangle(centreX - thumbSize * 0.5f, sliderPos - 3.0f,
                                thumbSize, 6.0f, 3.0f);
        g.setColour(fillColour);
        g.fillRoundedRectangle(centreX - 3.0f, sliderPos - 1.5f, 6.0f, 3.0f, 1.5f);
    } else {
        LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, sliderPos, minSliderPos, maxSliderPos, style, slider);
    }
}

void ClonadaLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& bg, bool highlighted, bool down) {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto cornerSize = 6.0f;

    auto baseColour = bg;
    if (down)
        baseColour = baseColour.brighter(0.1f);
    else if (highlighted)
        baseColour = baseColour.brighter(0.05f);

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, cornerSize);

    // Subtle top highlight
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.fillRoundedRectangle(bounds.removeFromTop(bounds.getHeight() * 0.5f), cornerSize);
}

void ClonadaLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool down,
                                       int, int, int, int, juce::ComboBox& box) {
    auto bounds = juce::Rectangle<float>(0, 0, (float)w, (float)h);
    auto cornerSize = 6.0f;

    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, cornerSize);

    auto outlineColour = box.findColour(juce::ComboBox::outlineColourId);
    g.setColour(down ? outlineColour.brighter(0.2f) : outlineColour);
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.0f);

    // Arrow
    auto arrowZone = bounds.removeFromRight(24.0f).reduced(7.0f, 9.0f);
    juce::Path arrow;
    arrow.addTriangle(arrowZone.getX(), arrowZone.getY(),
                      arrowZone.getCentreX(), arrowZone.getBottom(),
                      arrowZone.getRight(), arrowZone.getY());
    g.setColour(juce::Colour(kTextDim));
    g.fillPath(arrow);
}

void ClonadaLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                            bool highlighted, bool down) {
    auto bounds = button.getLocalBounds().toFloat();
    auto toggleArea = bounds.removeFromLeft(bounds.getHeight()).reduced(4.0f);

    // Toggle track
    auto trackBounds = toggleArea.reduced(2.0f);
    auto isOn = button.getToggleState();

    g.setColour(isOn ? juce::Colour(kRed).withAlpha(0.3f) : juce::Colour(kKnobTrack));
    g.fillRoundedRectangle(trackBounds, trackBounds.getHeight() * 0.5f);

    // Toggle thumb
    auto thumbSize = trackBounds.getHeight() - 4.0f;
    auto thumbX = isOn ? trackBounds.getRight() - thumbSize - 2.0f : trackBounds.getX() + 2.0f;
    g.setColour(isOn ? juce::Colour(kRed) : juce::Colour(kTextDim));
    g.fillEllipse(thumbX, trackBounds.getCentreY() - thumbSize * 0.5f, thumbSize, thumbSize);

    // Label
    g.setColour(button.findColour(juce::ToggleButton::textColourId));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(button.getButtonText(), bounds.reduced(4.0f, 0.0f), juce::Justification::centredLeft);
}

juce::Font ClonadaLookAndFeel::getComboBoxFont(juce::ComboBox&) {
    return juce::Font(juce::FontOptions(13.0f));
}

juce::Font ClonadaLookAndFeel::getTextButtonFont(juce::TextButton&, int) {
    return juce::Font(juce::FontOptions(12.0f));
}
