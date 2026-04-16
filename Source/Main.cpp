#include <JuceHeader.h>
#if JUCE_WINDOWS
# include <windows.h>
# include <dwmapi.h>
# pragma comment(lib, "dwmapi.lib")
#endif
#include "MainComponent.h"

class ConvolverApplication : public juce::JUCEApplication
{
public:
    ConvolverApplication()
    {
        auto exeFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        auto logFile = exeFile.getParentDirectory().getChildFile ("convolver_runtime.log");
        fileLogger = std::make_unique<juce::FileLogger> (logFile, "Convolver Runtime Log", 0);
        juce::Logger::setCurrentLogger (fileLogger.get());
    }

    const juce::String getApplicationName() override    { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override           { return true; }

    void initialise (const juce::String& /*commandLine*/) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
        juce::Logger::setCurrentLogger (nullptr);
        fileLogger.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String& /*commandLine*/) override {}

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            if (auto* content = getContentComponent())
            {
                setResizeLimits (content->getWidth(), content->getHeight(), 10000, 10000);
                centreWithSize (content->getWidth(), content->getHeight());
            }
           #endif

            // On Windows, request the native titlebar to use the system dark theme
           #if JUCE_WINDOWS
            if (auto* hwnd = static_cast<HWND> (getWindowHandle()))
            {
                // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 on newer Windows 10/11, 19 on older builds
                BOOL useDark = TRUE;
                HRESULT hr = DwmSetWindowAttribute (hwnd, 20, &useDark, sizeof (useDark));
                if (FAILED (hr))
                    DwmSetWindowAttribute (hwnd, 19, &useDark, sizeof (useDark));
            }
           #endif

            setIcon (createAppIcon());
            if (auto* peer = getPeer())
                peer->setIcon (createAppIcon());

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        static juce::Image createAppIcon()
        {
            const int size = 32;
            juce::Image img (juce::Image::ARGB, size, size, true);
            juce::Graphics g (img);

            // Background circle
            g.setColour (juce::Colour (0xff3a3a5c));
            g.fillEllipse (0.0f, 0.0f, (float) size, (float) size);

            // Draw a small waveform symbol
            juce::Path wave;
            const float cx = size * 0.5f;
            const float cy = size * 0.5f;
            const float amp = size * 0.25f;
            const int steps = 40;

            wave.startNewSubPath (size * 0.15f, cy);
            for (int i = 1; i <= steps; ++i)
            {
                float t = (float) i / (float) steps;
                float x = size * 0.15f + t * size * 0.7f;
                float envelope = std::sin (t * juce::MathConstants<float>::pi);
                float y = cy + std::sin (t * juce::MathConstants<float>::twoPi * 3.0f) * amp * envelope;
                wave.lineTo (x, y);
            }

            g.setColour (juce::Colours::white);
            g.strokePath (wave, juce::PathStrokeType (1.8f));

            return img;
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<juce::FileLogger> fileLogger;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (ConvolverApplication)
