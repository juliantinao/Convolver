#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize (800, 600);
}

MainComponent::~MainComponent() = default;

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setFont (juce::FontOptions (24.0f));
    g.setColour (juce::Colours::white);
    g.drawText ("Convolver Batch WAV Convolution",
                getLocalBounds(), juce::Justification::centred, true);
}

void MainComponent::resized()
{
}
