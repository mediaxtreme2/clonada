#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay() {
    startTimerHz(30);
}

void WaveformDisplay::pushSample(float inputSample, float outputSample) {
    auto pos = writePos_.load() % kBufferSize;
    inputBuffer_[pos] = inputSample;
    outputBuffer_[pos] = outputSample;
    writePos_.store(pos + 1);
}

void WaveformDisplay::setColours(juce::Colour input, juce::Colour output) {
    inputColour_ = input;
    outputColour_ = output;
}

void WaveformDisplay::timerCallback() {
    repaint();
}

void WaveformDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    auto w = bounds.getWidth();
    auto h = bounds.getHeight();
    auto midY = h * 0.5f;

    // Background
    g.setColour(juce::Colour(0xFF0c0c14));
    g.fillRoundedRectangle(bounds, 6.0f);

    // Center line
    g.setColour(juce::Colour(0xFF1e293b));
    g.drawHorizontalLine((int)midY, bounds.getX() + 2.0f, bounds.getRight() - 2.0f);

    // Grid lines
    g.setColour(juce::Colour(0xFF141420));
    for (int i = 1; i < 4; ++i) {
        float y = midY + (h * 0.25f * i * 0.5f);
        float y2 = midY - (h * 0.25f * i * 0.5f);
        g.drawHorizontalLine((int)y, bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
        g.drawHorizontalLine((int)y2, bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
    }

    auto currentPos = writePos_.load();

    auto drawWaveform = [&](const std::array<float, kBufferSize>& buffer, juce::Colour colour, float alpha) {
        juce::Path path;
        bool started = false;

        for (int i = 0; i < kBufferSize; ++i) {
            int idx = (currentPos + i) % kBufferSize;
            float x = bounds.getX() + 2.0f + (w - 4.0f) * (float)i / (float)kBufferSize;
            float sample = juce::jlimit(-1.0f, 1.0f, buffer[idx]);
            float y = midY - sample * (h * 0.4f);

            if (!started) {
                path.startNewSubPath(x, y);
                started = true;
            } else {
                path.lineTo(x, y);
            }
        }

        // Glow
        g.setColour(colour.withAlpha(alpha * 0.15f));
        g.strokePath(path, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved));

        // Main line
        g.setColour(colour.withAlpha(alpha));
        g.strokePath(path, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));
    };

    drawWaveform(inputBuffer_, inputColour_, 0.5f);
    drawWaveform(outputBuffer_, outputColour_, 0.85f);

    // Labels
    g.setFont(juce::FontOptions(9.0f));
    g.setColour(inputColour_.withAlpha(0.6f));
    g.drawText("IN", bounds.reduced(6.0f, 3.0f), juce::Justification::topLeft);
    g.setColour(outputColour_.withAlpha(0.6f));
    g.drawText("OUT", bounds.reduced(6.0f, 3.0f), juce::Justification::topRight);

    // Border
    g.setColour(juce::Colour(0xFF1e1e36));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 0.5f);
}
