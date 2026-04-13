#pragma once

#include <JuceHeader.h>

class FileListModel;
class DarkLookAndFeel;

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

    std::unique_ptr<FileListModel> fileListModel;
    std::unique_ptr<DarkLookAndFeel> darkLookAndFeel;
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

    juce::TextButton helpButton { "Help" };
    juce::TextButton convolveButton { "Convolve" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
