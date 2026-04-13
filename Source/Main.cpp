#include <JuceHeader.h>
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

            setVisible (true);
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
