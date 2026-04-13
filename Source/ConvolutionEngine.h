#pragma once

#include <JuceHeader.h>

#include <functional>

//==============================================================================
/**
    Performs frequency-domain convolution of a WAV file with an impulse response,
    following the Phase C deconvolution pattern from the Farina ESS pipeline:
    zero-pad both signals, forward FFT, complex multiply, inverse FFT.
*/
class ConvolutionEngine
{
public:
    struct FilePair
    {
        juce::File inputFile;
        juce::File outputFile;
    };

    /**
        Convolves a single WAV input file with an impulse response WAV file and
        writes the result to outputFile. Normalises the output to 0.99 peak.

        @param inputFile     The WAV file to convolve.
        @param irFile        The impulse response (or convolution kernel) WAV file.
        @param outputFile    Destination WAV file for the convolved result.
        @return              An empty string on success, or an error description.
    */
    static juce::String processFile (const juce::File& inputFile,
                                     const juce::File& irFile,
                                     const juce::File& outputFile);

    /**
        Convolves a batch of WAV files with a single impulse response.

        @param filePairs              Input/output file pairs.
        @param irFile                 The impulse response WAV file.
        @param maintainRelativeLevels If true, the loudest result is normalised
                                      to 0.99 and the same gain is applied to all
                                      others, preserving relative levels. If false,
                                      each result is normalised to 0.99 independently.
        @param errors                 Receives one entry per failure.
        @return                       The number of successfully written files.
    */
    static int processBatch (const std::vector<FilePair>& filePairs,
                             const juce::File& irFile,
                             bool maintainRelativeLevels,
                              juce::StringArray& errors,
                              std::function<void (double)> progressCallback = {});

private:
    ConvolutionEngine() = delete;
};
