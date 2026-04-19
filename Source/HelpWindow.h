#pragma once

#include <JuceHeader.h>

class HelpContentComponent : public juce::Component
{
public:
    HelpContentComponent();

    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::TextEditor helpTextEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpContentComponent)
};

class HelpWindow : public juce::DocumentWindow
{
public:
    explicit HelpWindow (juce::Component* ownerComponent);

    void closeButtonPressed() override;

private:
    juce::Component::SafePointer<juce::Component> owner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpWindow)
};