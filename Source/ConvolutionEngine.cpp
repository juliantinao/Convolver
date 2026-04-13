#include "ConvolutionEngine.h"

//==============================================================================
// Helper: load a WAV file into an AudioBuffer (returns empty buffer on failure).
static juce::AudioBuffer<float> loadWavFile (const juce::File& file,
                                             double& sampleRateOut,
                                             juce::String& errorOut)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        errorOut = "Could not open file: " + file.getFullPathName();
        return {};
    }

    sampleRateOut = reader->sampleRate;

    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels),
                                     static_cast<int> (reader->lengthInSamples));
    reader->read (&buffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);
    return buffer;
}

//==============================================================================
juce::String ConvolutionEngine::processFile (const juce::File& inputFile,
                                             const juce::File& irFile,
                                             const juce::File& outputFile)
{
    //--------------------------------------------------------------------------
    // 1. Read input and IR WAV files
    //--------------------------------------------------------------------------
    juce::String inputError, irError;
    double inputSampleRate = 0.0;
    double irSampleRate = 0.0;

    auto inputBuffer = loadWavFile (inputFile, inputSampleRate, inputError);
    if (inputBuffer.getNumSamples() == 0)
        return inputError;

    auto irBuffer = loadWavFile (irFile, irSampleRate, irError);
    if (irBuffer.getNumSamples() == 0)
        return irError;

    //--------------------------------------------------------------------------
    // 2. Determine FFT size (next power of two >= input + IR - 1)
    //    This follows the Phase C pattern: zero-pad to N_fft.
    //--------------------------------------------------------------------------
    const int inputLength = inputBuffer.getNumSamples();
    const int irLength = irBuffer.getNumSamples();
    const int convLength = inputLength + irLength - 1;

    // juce::dsp::FFT requires the order (log2 of the size)
    const int fftOrder = static_cast<int> (std::ceil (std::log2 (static_cast<double> (convLength))));
    const int fftSize = 1 << fftOrder;

    juce::dsp::FFT fft (fftOrder);

    // Number of output channels: max of input and IR channels
    const int numOutputChannels = juce::jmax (inputBuffer.getNumChannels(),
                                              irBuffer.getNumChannels());

    // Output buffer for the final convolved result
    juce::AudioBuffer<float> outputBuffer (numOutputChannels, convLength);
    outputBuffer.clear();

    //--------------------------------------------------------------------------
    // 3. Per-channel convolution via frequency-domain multiplication
    //    Phase C steps: C.1 zero-pad, C.3 forward FFT, C.4 multiply, C.5 IFFT
    //--------------------------------------------------------------------------
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        // Pick the channel to read from each buffer, clamping to available channels
        const int inputCh = juce::jmin (ch, inputBuffer.getNumChannels() - 1);
        const int irCh = juce::jmin (ch, irBuffer.getNumChannels() - 1);

        // C.1 / C.2 — Zero-pad input and IR to fftSize * 2
        // juce::dsp::FFT::performRealOnlyForwardTransform expects an array of
        // size fftSize * 2 (interleaved real/imag pairs).
        std::vector<float> inputFFTData (static_cast<size_t> (fftSize) * 2, 0.0f);
        std::vector<float> irFFTData (static_cast<size_t> (fftSize) * 2, 0.0f);

        // Copy input samples into FFT buffer (zero-padded beyond inputLength)
        std::memcpy (inputFFTData.data(),
                     inputBuffer.getReadPointer (inputCh),
                     sizeof (float) * static_cast<size_t> (inputLength));

        // Copy IR samples into FFT buffer (zero-padded beyond irLength)
        std::memcpy (irFFTData.data(),
                     irBuffer.getReadPointer (irCh),
                     sizeof (float) * static_cast<size_t> (irLength));

        // C.3 — Forward FFT of both signals
        fft.performRealOnlyForwardTransform (inputFFTData.data());
        fft.performRealOnlyForwardTransform (irFFTData.data());

        // C.4 — Frequency-domain complex multiplication
        // Data layout after forward transform: pairs of (real, imag) for each bin.
        for (int k = 0; k < fftSize; ++k)
        {
            const int idx = k * 2;
            const float realA = inputFFTData[static_cast<size_t> (idx)];
            const float imagA = inputFFTData[static_cast<size_t> (idx + 1)];
            const float realB = irFFTData[static_cast<size_t> (idx)];
            const float imagB = irFFTData[static_cast<size_t> (idx + 1)];

            // (a + bi)(c + di) = (ac - bd) + (ad + bc)i
            inputFFTData[static_cast<size_t> (idx)]     = realA * realB - imagA * imagB;
            inputFFTData[static_cast<size_t> (idx + 1)] = realA * imagB + imagA * realB;
        }

        // C.5 — Inverse FFT
        fft.performRealOnlyInverseTransform (inputFFTData.data());

        // Copy convolution result (only convLength samples are meaningful)
        auto* dest = outputBuffer.getWritePointer (ch);

        for (int n = 0; n < convLength; ++n)
            dest[n] = inputFFTData[static_cast<size_t> (n)];
    }

    //--------------------------------------------------------------------------
    // 4. Normalise output to avoid clipping
    //--------------------------------------------------------------------------
    float peakLevel = 0.0f;

    for (int ch = 0; ch < numOutputChannels; ++ch)
        peakLevel = juce::jmax (peakLevel, outputBuffer.getMagnitude (ch, 0, convLength));

    if (peakLevel > 0.0f)
    {
        const float scale = 0.9f / peakLevel;
        outputBuffer.applyGain (scale);
    }

    //--------------------------------------------------------------------------
    // 5. Write result to output WAV file
    //--------------------------------------------------------------------------
    if (outputFile.exists())
        outputFile.deleteFile();

    std::unique_ptr<juce::OutputStream> fileStream (outputFile.createOutputStream());

    if (fileStream == nullptr)
        return "Could not create output file: " + outputFile.getFullPathName();

    juce::WavAudioFormat wavFormat;
    auto writerOptions = juce::AudioFormatWriterOptions()
                             .withSampleRate (inputSampleRate)
                             .withNumChannels (numOutputChannels)
                             .withBitsPerSample (16);

    auto writer = wavFormat.createWriterFor (fileStream, writerOptions);

    if (writer == nullptr)
        return "Could not create WAV writer for: " + outputFile.getFullPathName();

    // Writer now owns the stream (fileStream was moved)

    if (! writer->writeFromAudioSampleBuffer (outputBuffer, 0, convLength))
        return "Failed to write audio data to: " + outputFile.getFullPathName();

    return {};
}
