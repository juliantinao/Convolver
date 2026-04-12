#pragma once

#include <JuceHeader.h>

class FileListRootItem;

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

    void addFiles (const juce::Array<juce::File>& filesToAdd);
    void removeSelectedFiles();
    void refreshFileList();

    friend class FileListRootItem;

    std::unique_ptr<FileListRootItem> fileListRootItem;
    juce::GroupComponent fileTreeBorder;
    juce::TreeView fileTreeView;
    juce::TextButton addButton { "Add" };
    juce::TextButton removeButton { "Remove" };
    std::vector<FileEntry> fileEntries;
    int nextFileId = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
