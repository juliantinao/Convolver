#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Performs frequency-domain convolution of a WAV file with an impulse response,
    following the Phase C deconvolution pattern from the Farina ESS pipeline:
    zero-pad both signals, forward FFT, complex multiply, inverse FFT.
*/
class ConvolutionEngine
{
public:
    /**
        Convolves a single WAV input file with an impulse response WAV file and
        writes the result to outputFile.

        @param inputFile     The WAV file to convolve.
        @param irFile        The impulse response (or convolution kernel) WAV file.
        @param outputFile    Destination WAV file for the convolved result.
        @return              An empty string on success, or an error description.
    */
    static juce::String processFile (const juce::File& inputFile,
                                     const juce::File& irFile,
                                     const juce::File& outputFile);

private:
    ConvolutionEngine() = delete;
};
