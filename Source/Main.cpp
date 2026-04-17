#include <JuceHeader.h>
#if JUCE_WINDOWS
# include <windows.h>
# include <dwmapi.h>
# pragma comment(lib, "dwmapi.lib")
#endif
#include "MainComponent.h"
#include "AppIcon.h"

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
            auto* mainComponent = new MainComponent();

            setUsingNativeTitleBar (true);
            setContentOwned (mainComponent, true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            const auto contentBounds = mainComponent->getLocalBounds();
            const auto minWidth = contentBounds.getWidth();
            const auto minHeight = contentBounds.getHeight();

            setResizable (true, false);
            setResizeLimits (minWidth, minHeight, 10000, 10000);
            centreWithSize (minWidth, minHeight);
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

            const auto appIcon = createAppIcon();
            setIcon (appIcon);

            setVisible (true);

            if (auto* peer = getPeer())
                peer->setIcon (appIcon);

           #if ! (JUCE_IOS || JUCE_ANDROID)
            setResizeLimits (minWidth, minHeight, 10000, 10000);
           #endif
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<juce::FileLogger> fileLogger;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (ConvolverApplication)
