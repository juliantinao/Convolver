#include "ConvolutionEngine.h"

namespace
{
struct LoadedWavFile
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 0.0;
    int bitsPerSample = 16;
};

static int sanitiseWavBitsPerSample (int bitsPerSample)
{
    if (bitsPerSample <= 8)
        return 8;

    if (bitsPerSample <= 16)
        return 16;

    if (bitsPerSample <= 24)
        return 24;

    return 32;
}
}

//==============================================================================
// Helper: load a WAV file into an AudioBuffer (returns empty buffer on failure).
static LoadedWavFile loadWavFile (const juce::File& file,
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

    LoadedWavFile loadedFile;
    loadedFile.sampleRate = reader->sampleRate;
    loadedFile.bitsPerSample = sanitiseWavBitsPerSample (reader->bitsPerSample);
    loadedFile.buffer.setSize (static_cast<int> (reader->numChannels),
                               static_cast<int> (reader->lengthInSamples));
    reader->read (&loadedFile.buffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);
    return loadedFile;
}

//==============================================================================
// Helper: convolve an input buffer with an IR buffer via frequency-domain
// multiplication (Phase C pattern). Returns the convolved AudioBuffer.
static juce::AudioBuffer<float> convolveBuffers (const juce::AudioBuffer<float>& inputBuffer,
                                                  const juce::AudioBuffer<float>& irBuffer)
{
    const int inputLength = inputBuffer.getNumSamples();
    const int irLength = irBuffer.getNumSamples();
    const int convLength = inputLength + irLength - 1;

    const int fftOrder = static_cast<int> (std::ceil (std::log2 (static_cast<double> (convLength))));
    const int fftSize = 1 << fftOrder;

    juce::dsp::FFT fft (fftOrder);

    const int numOutputChannels = juce::jmax (inputBuffer.getNumChannels(),
                                              irBuffer.getNumChannels());

    juce::AudioBuffer<float> outputBuffer (numOutputChannels, convLength);
    outputBuffer.clear();

    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        const int inputCh = juce::jmin (ch, inputBuffer.getNumChannels() - 1);
        const int irCh = juce::jmin (ch, irBuffer.getNumChannels() - 1);

        // C.1 / C.2 — Zero-pad input and IR to fftSize * 2
        std::vector<float> inputFFTData (static_cast<size_t> (fftSize) * 2, 0.0f);
        std::vector<float> irFFTData (static_cast<size_t> (fftSize) * 2, 0.0f);

        std::memcpy (inputFFTData.data(),
                     inputBuffer.getReadPointer (inputCh),
                     sizeof (float) * static_cast<size_t> (inputLength));

        std::memcpy (irFFTData.data(),
                     irBuffer.getReadPointer (irCh),
                     sizeof (float) * static_cast<size_t> (irLength));

        // C.3 — Forward FFT of both signals
        fft.performRealOnlyForwardTransform (inputFFTData.data());
        fft.performRealOnlyForwardTransform (irFFTData.data());

        // C.4 — Frequency-domain complex multiplication
        for (int k = 0; k < fftSize; ++k)
        {
            const int idx = k * 2;
            const float realA = inputFFTData[static_cast<size_t> (idx)];
            const float imagA = inputFFTData[static_cast<size_t> (idx + 1)];
            const float realB = irFFTData[static_cast<size_t> (idx)];
            const float imagB = irFFTData[static_cast<size_t> (idx + 1)];

            inputFFTData[static_cast<size_t> (idx)]     = realA * realB - imagA * imagB;
            inputFFTData[static_cast<size_t> (idx + 1)] = realA * imagB + imagA * realB;
        }

        // C.5 — Inverse FFT
        fft.performRealOnlyInverseTransform (inputFFTData.data());

        auto* dest = outputBuffer.getWritePointer (ch);

        for (int n = 0; n < convLength; ++n)
            dest[n] = inputFFTData[static_cast<size_t> (n)];
    }

    return outputBuffer;
}

//==============================================================================
// Helper: find peak magnitude across all channels of a buffer.
static float getPeakLevel (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));

    return peak;
}

//==============================================================================
// Helper: write an AudioBuffer to a WAV file.
static juce::String writeWavFile (const juce::AudioBuffer<float>& buffer,
                                  double sampleRate,
                                  int bitsPerSample,
                                  const juce::File& outputFile)
{
    if (outputFile.exists())
        outputFile.deleteFile();

    std::unique_ptr<juce::OutputStream> fileStream (outputFile.createOutputStream());

    if (fileStream == nullptr)
        return "Could not create output file: " + outputFile.getFullPathName();

    juce::WavAudioFormat wavFormat;
    auto writerOptions = juce::AudioFormatWriterOptions()
                             .withSampleRate (sampleRate)
                             .withNumChannels (buffer.getNumChannels())
                             .withBitsPerSample (sanitiseWavBitsPerSample (bitsPerSample));

    auto writer = wavFormat.createWriterFor (fileStream, writerOptions);

    if (writer == nullptr)
        return "Could not create WAV writer for: " + outputFile.getFullPathName();

    if (! writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()))
        return "Failed to write audio data to: " + outputFile.getFullPathName();

    return {};
}

//==============================================================================
juce::String ConvolutionEngine::processFile (const juce::File& inputFile,
                                             const juce::File& irFile,
                                             const juce::File& outputFile)
{
    juce::StringArray errors;
    processBatch ({ { inputFile, outputFile } }, irFile, false, errors);

    if (errors.isEmpty())
        return {};

    return errors[0];
}

//==============================================================================
int ConvolutionEngine::processBatch (const std::vector<FilePair>& filePairs,
                                     const juce::File& irFile,
                                     bool maintainRelativeLevels,
                                     juce::StringArray& errors,
                                     std::function<void (double)> progressCallback)
{
    const auto reportProgress = [&] (double progress)
    {
        if (progressCallback)
            progressCallback (juce::jlimit (0.0, 1.0, progress));
    };

    reportProgress (0.0);

    // Load the IR once for the whole batch
    juce::String irError;
    auto irData = loadWavFile (irFile, irError);

    if (irData.buffer.getNumSamples() == 0)
    {
        errors.add ("IR: " + irError);
        return 0;
    }

    struct ConvolvedResult
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 0.0;
        int bitsPerSample = 16;
        juce::File outputFile;
        float peakLevel = 0.0f;
    };

    std::vector<ConvolvedResult> results;
    results.reserve (filePairs.size());
    int successCount = 0;
    int processedInputs = 0;
    const int totalInputs = juce::jmax (1, static_cast<int> (filePairs.size()));

    //--------------------------------------------------------------------------
    // Phase 1: convolve all files into memory buffers
    //--------------------------------------------------------------------------
    for (const auto& pair : filePairs)
    {
        juce::String inputError;
        auto inputData = loadWavFile (pair.inputFile, inputError);

        if (inputData.buffer.getNumSamples() == 0)
        {
            errors.add (pair.inputFile.getFileName() + ": " + inputError);
            ++processedInputs;
            reportProgress (0.5 * (static_cast<double> (processedInputs) / totalInputs));
            continue;
        }

        auto convolved = convolveBuffers (inputData.buffer, irData.buffer);
        float peak = getPeakLevel (convolved);

        results.push_back ({ std::move (convolved), inputData.sampleRate,
                             inputData.bitsPerSample,
                             pair.outputFile, peak });

        ++processedInputs;
        reportProgress (0.5 * (static_cast<double> (processedInputs) / totalInputs));
    }

    //--------------------------------------------------------------------------
    // Phase 2: normalise
    //--------------------------------------------------------------------------
    if (maintainRelativeLevels)
    {
        // Find the global peak across all convolved results
        float globalPeak = 0.0f;

        for (const auto& r : results)
            globalPeak = juce::jmax (globalPeak, r.peakLevel);

        // Apply the same gain to every result so the loudest hits 0.99
        if (globalPeak > 0.0f)
        {
            const float scale = 0.99f / globalPeak;

            for (auto& r : results)
                r.buffer.applyGain (scale);
        }
    }
    else
    {
        // Normalise each result independently to 0.99
        for (auto& r : results)
        {
            if (r.peakLevel > 0.0f)
            {
                const float scale = 0.99f / r.peakLevel;
                r.buffer.applyGain (scale);
            }
        }
    }

    //--------------------------------------------------------------------------
    // Phase 3: write all results to disk
    //--------------------------------------------------------------------------
    int processedWrites = 0;
    const int totalWrites = juce::jmax (1, static_cast<int> (results.size()));

    for (const auto& r : results)
    {
        auto writeError = writeWavFile (r.buffer, r.sampleRate, r.bitsPerSample, r.outputFile);

        if (writeError.isEmpty())
            ++successCount;
        else
            errors.add (r.outputFile.getFileName() + ": " + writeError);

        ++processedWrites;
        reportProgress (0.5 + (0.5 * (static_cast<double> (processedWrites) / totalWrites)));
    }

    reportProgress (1.0);
    return successCount;
}
