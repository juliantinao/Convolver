// Small programmatic application icon for title bar
#pragma once

#include <JuceHeader.h>

inline juce::Image createAppIcon()
{
    const int size = 32;
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);

    // Background circle
    g.setColour (juce::Colour (0xff3a3a5c));
    g.fillEllipse (0.0f, 0.0f, (float) size, (float) size);

    // Draw a small waveform symbol
    juce::Path wave;
    const float cx = size * 0.5f;
    const float cy = size * 0.5f;
    const float amp = size * 0.25f;
    const int steps = 40;

    wave.startNewSubPath (size * 0.15f, cy);
    for (int i = 1; i <= steps; ++i)
    {
        float t = (float) i / (float) steps;
        float x = size * 0.15f + t * size * 0.7f;
        float envelope = std::sin (t * juce::MathConstants<float>::pi);
        float y = cy + std::sin (t * juce::MathConstants<float>::twoPi * 3.0f) * amp * envelope;
        wave.lineTo (x, y);
    }

    g.setColour (juce::Colours::white);
    g.strokePath (wave, juce::PathStrokeType (1.8f));

    return img;
}
