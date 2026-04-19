#pragma once
#include "ConvolutionEngine.h"

#include <JuceHeader.h>

#include <atomic>
#include <thread>

class FileListModel;
class DarkLookAndFeel;
class ConvolveButtonLookAndFeel;

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct FileEntry
    {
        int id = 0;
        juce::File file;
    };

    friend class FileListModel;

    void addFiles (const juce::Array<juce::File>& filesToAdd);
    void removeSelectedFiles();
    void refreshFileList();
    void startConvolutionBatch (std::vector<ConvolutionEngine::FilePair> filePairs);
    void setConvolveProgress (float progress);

    std::unique_ptr<FileListModel> fileListModel;
    std::unique_ptr<DarkLookAndFeel> darkLookAndFeel;
    std::unique_ptr<ConvolveButtonLookAndFeel> convolveButtonLookAndFeel;
    juce::GroupComponent fileListBorder;
    juce::ListBox fileListBox;
    juce::TextButton addButton { "Add" };
    juce::TextButton removeButton { "Remove" };
    std::vector<FileEntry> fileEntries;
    int nextFileId = 1;

    juce::GroupComponent convolveFileBorder;
    juce::Label convolveFileLabel;
    juce::TextButton convolveFileSelectButton { "Select" };
    juce::File convolveFile;

    juce::GroupComponent outputDirBorder;
    juce::Label outputDirLabel;
    juce::TextButton outputDirSelectButton { "Select" };
    juce::File outputDirectory;

    juce::GroupComponent prefixBorder;
    juce::Label prefixLabel;
    juce::TextEditor prefixEditor;

    juce::ToggleButton maintainLevelToggle { "Maintain relative level between files." };

    juce::TextButton helpButton { "Help" };
    juce::TextButton convolveButton { "Convolve" };
    std::atomic<bool> convolving { false };
    std::jthread convolveWorker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
