#pragma once
#include <JuceHeader.h>

class DarkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DarkLookAndFeel();
    ~DarkLookAndFeel() override = default;

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
};

class ConvolveButtonLookAndFeel : public DarkLookAndFeel
{
public:
    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
};
