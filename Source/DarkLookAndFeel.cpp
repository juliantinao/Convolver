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
