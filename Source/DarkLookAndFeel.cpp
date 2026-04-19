#include "DarkLookAndFeel.h"

DarkLookAndFeel::DarkLookAndFeel()
{
    // Dark mode color scheme
    auto darkBg = juce::Colour::fromRGB (33, 33, 33);
    auto panelBg = juce::Colour::fromRGB (28, 28, 28);
    auto outlineCol = juce::Colour::fromRGB (64, 64, 64);
    auto controlCol = juce::Colour::fromRGB (45, 45, 45);
    auto textCol = juce::Colours::white;

    setColour (juce::ResizableWindow::backgroundColourId, darkBg);
    setColour (juce::ListBox::backgroundColourId, panelBg);
    setColour (juce::GroupComponent::outlineColourId, outlineCol);
    setColour (juce::GroupComponent::textColourId, textCol);
    setColour (juce::Label::textColourId, textCol);
    setColour (juce::TextButton::buttonColourId, controlCol);
    setColour (juce::TextButton::textColourOffId, textCol);
    setColour (juce::AlertWindow::backgroundColourId, darkBg);
    setColour (juce::AlertWindow::textColourId, textCol);
    setColour (juce::AlertWindow::outlineColourId, outlineCol);
    setColour (juce::PopupMenu::backgroundColourId, panelBg);
    setColour (juce::PopupMenu::textColourId, textCol);
}

void DarkLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                            juce::Button& button,
                                            const juce::Colour& backgroundColour,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    auto cornerSize = juce::jmin (bounds.getHeight() * 0.5f, 6.0f);

    g.setColour (backgroundColour);
    g.fillRoundedRectangle (bounds, cornerSize);

    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
    {
        g.setColour (juce::Colours::white.withAlpha (shouldDrawButtonAsDown ? 0.14f : 0.08f));
        g.fillRoundedRectangle (bounds, cornerSize);
    }

    g.setColour (button.findColour (juce::GroupComponent::outlineColourId));
    g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
}

void ConvolveButtonLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                      juce::Button& button,
                                                      const juce::Colour& backgroundColour,
                                                      bool shouldDrawButtonAsHighlighted,
                                                      bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    auto cornerSize = juce::jmin (bounds.getHeight() * 0.5f, 6.0f);

    const bool isProcessing = button.getProperties().getWithDefault ("convolveProcessing", false);
    const float progress = juce::jlimit (0.0f, 1.0f, (float) button.getProperties().getWithDefault ("convolveProgress", 0.0f));

    auto highlightColour = backgroundColour.interpolatedWith (juce::Colours::white, 0.42f);

    g.setColour (backgroundColour);
    g.fillRoundedRectangle (bounds, cornerSize);

    if (isProcessing && progress > 0.0f)
    {
        auto progressBounds = bounds;
        progressBounds.setWidth (bounds.getWidth() * progress);

        g.setColour (highlightColour);
        g.fillRoundedRectangle (progressBounds, cornerSize);
    }

    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
    {
        g.setColour (juce::Colours::white.withAlpha (shouldDrawButtonAsDown ? 0.14f : 0.08f));
        g.fillRoundedRectangle (bounds, cornerSize);
    }

    g.setColour (button.findColour (juce::GroupComponent::outlineColourId));
    g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
}
