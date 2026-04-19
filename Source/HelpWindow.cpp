#include "HelpWindow.h"

namespace
{
juce::String buildHelpText()
{
    juce::StringArray lines;

    lines.add ("# What this app does:");
    lines.add("");
    lines.add ("Convolver is an offline batch WAV convolution tool. You choose one WAV file that acts as the impulse response (IR), then apply that IR to many other WAV files in one pass.");
    lines.add ("");
    lines.add("# How to use the app");
    lines.add("1. 'Add' to choose one or more source WAV files to process.");
    lines.add("2. 'With this file' to choose the IR (Impulse Response) WAV you want to use. This can be any .wav file.");
    lines.add("3. 'Output Path' to choose where the rendered WAV files should be written.");
    lines.add("4. 'Output Prefix' if you want the new files names to start with a label such as conv_ or roomA_. This prefix will be added to the original source name.");
    lines.add("5. Optionally remove highlighted files with 'Remove'. You can select multiple files from the list using ctrl or shift");
    lines.add("6. Decide whether to keep the 'Maintain relative level between files' option enabled. (See detail below)");
    lines.add("7. Click 'Convolve' to process the batch.");
    lines.add("");
    lines.add("Maintain relative level between files:");
    lines.add("This option changes how final peak normalization is handled across the whole batch.");
    lines.add("");
    lines.add("When enabled:");
    lines.add("- The app first convolves every file.");
    lines.add("- It finds the single highest peak across all convolved results.");
    lines.add("- It applies one shared gain factor so that the loudest output reaches 0.99 peak.");
    lines.add("- Every other file receives that same gain, so the level differences between files are preserved.");
    lines.add("");
    lines.add("This is usually the best choice when the input files are related to each other, such as album tracks, stem exports, drum multisamples, dialogue takes, or measurement sets where their original level relationships matter.");
    lines.add("");
    lines.add("When disabled:");
    lines.add("- Each convolved file is normalised independently to 0.99 peak.");
    lines.add("- That can make quiet source files come out nearly as loud as hot source files.");
    lines.add("- Relative level differences between files are no longer preserved.");
    lines.add("");
    lines.add("Turn it off when every file is meant to stand on its own and you want each output to use as much headroom as possible.");
    lines.add("");
    lines.add("");
    lines.add ("# Why FFT Convolution");
    lines.add ("Convolution in the time domain means every input sample interacts with every sample in the IR. That is mathematically correct but expensive for long audio files and long reverbs.");
    lines.add ("");
    lines.add ("This app uses FFT-based convolution instead:");
    lines.add ("1. Read the source WAV and the IR (Impulse Response) WAV.");
    lines.add ("2. Zero-pad both signals so the convolution stays linear rather than circular.");
    lines.add ("3. Transform both signals into the frequency domain with the Fast Fourier Transform (FFT).");
    lines.add ("4. Multiply the complex spectra bin by bin.");
    lines.add ("5. Transform the result back to the time domain with the inverse FFT.");
    lines.add ("6. Normalize. (see Maintain relative level between files)");
    lines.add ("");
    lines.add ("That produces the same linear-convolution result much faster than a naive sample-by-sample implementation, especially when the IR is long.");
    lines.add ("");
    lines.add ("# Common use cases");
    lines.add ("- Reverb: convolve with a room, hall, plate, spring, or ambience IR to place dry sounds in a space.");
    lines.add ("- Speaker and cabinet simulation: convolve with measured loudspeaker, guitar cabinet, or microphone-chain IRs." );
    lines.add ("- Acoustic and electrical IR measurement: convolve log sweep recorded measurements with the inverse filter to obtain the respective IR's (See Farina's ESS Measurement Method paper)");
    lines.add ("- Sound design: convolve drums, voices, field recordings, drones, or noise with another .wav just to explore textures.");
    lines.add ("");
    lines.add ("# About Farina ESS measurements");
    lines.add ("A very common way to measure an Impulse Response is the Farina Exponential Sine Sweep (ESS) method.");
    lines.add ("");
    lines.add ("Brief description of the method:");
    lines.add ("- Generate a logarithmic sweep that moves from low frequency to high frequency with sufficient tail silence to accommodate for room reverb tail, generate also the corresponding inverse filter.");
    lines.add ("- Play that sweep through the system you want to measure, such as a room, loudspeaker, cabinet, or signal chain.");
    lines.add ("- Record the system response.");
    lines.add ("- Convolve the recording with the matching inverse filter.");
    lines.add ("- The result is an impulse response WAV that captures the linear behaviour of that system.");
    lines.add ("");
    lines.add ("Farina ESS is popular because the sweep energy improves signal-to-noise ratio, FFT-based deconvolution is efficient, and harmonic distortion tends to separate in time before the main linear impulse response instead of contaminating it the way circular methods can.");
    lines.add ("");
    lines.add ("Once you already have that measured IR as a WAV file, you can use it with this app like any other IR to apply it to another source.");
    lines.add ("");
    lines.add ("");
    lines.add ("# Tips");
    lines.add ("- Use good IRs. The output quality can never exceed the quality of the selected kernel.");
    lines.add ("- Very long IRs create very long rendered files because linear convolution adds tail length.");
    lines.add ("- If you are measuring IRs with ESS, keep sweep parameters and inverse-filter construction consistent, and avoid clipping during playback or recording.");
    lines.add ("- Experimental kernels can sound great even when they are not traditional impulse responses. Try short noises, impacts, resonant hits, or strange textures.");

    return lines.joinIntoString ("\n");
}
}

HelpContentComponent::HelpContentComponent()
{
    titleLabel.setText ("Convolver Help", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId,
                          findColour (juce::Label::textColourId));
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("FFT-based batch WAV convolution for reverbs, measurement IRs, and sound design.",
                           juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    subtitleLabel.setColour (juce::Label::textColourId,
                             findColour (juce::Label::textColourId).withAlpha (0.8f));
    addAndMakeVisible (subtitleLabel);

    helpTextEditor.setMultiLine (true);
    helpTextEditor.setReadOnly (true);
    helpTextEditor.setScrollbarsShown (true);
    helpTextEditor.setPopupMenuEnabled (true);
    helpTextEditor.setCaretVisible (false);
    helpTextEditor.setText (buildHelpText(), juce::dontSendNotification);
    helpTextEditor.setColour (juce::TextEditor::backgroundColourId,
                              findColour (juce::ListBox::backgroundColourId));
    helpTextEditor.setColour (juce::TextEditor::textColourId,
                              findColour (juce::Label::textColourId));
    helpTextEditor.setColour (juce::TextEditor::outlineColourId,
                              findColour (juce::GroupComponent::outlineColourId));
    helpTextEditor.setColour (juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (helpTextEditor);

    setSize (760, 640);
}

void HelpContentComponent::resized()
{
    auto area = getLocalBounds().reduced (14);

    titleLabel.setBounds (area.removeFromTop (30));
    subtitleLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);
    helpTextEditor.setBounds (area);
}

HelpWindow::HelpWindow (juce::Component* ownerComponent)
    : DocumentWindow ("Convolver Help",
                      juce::Desktop::getInstance().getDefaultLookAndFeel()
                          .findColour (juce::ResizableWindow::backgroundColourId),
                      juce::DocumentWindow::closeButton),
      owner (ownerComponent)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setResizeLimits (640, 500, 1400, 1600);
    setContentOwned (new HelpContentComponent(), true);
    setSize (780, 680);

    if (owner != nullptr)
        centreAroundComponent (owner.getComponent(), getWidth(), getHeight());
    else
        centreWithSize (getWidth(), getHeight());
}

void HelpWindow::closeButtonPressed()
{
    setVisible (false);
}