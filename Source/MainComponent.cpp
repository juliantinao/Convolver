#include "MainComponent.h"

#include <unordered_set>

namespace
{
class FileListItem : public juce::TreeViewItem
{
public:
    FileListItem (int idIn, juce::File fileIn)
        : id (idIn), file (std::move (fileIn))
    {
    }

    bool mightContainSubItems() override
    {
        return false;
    }

    juce::String getUniqueName() const override
    {
        return juce::String (id);
    }

    int getItemHeight() const override
    {
        return 24;
    }

    void paintItem (juce::Graphics& g, int width, int height) override
    {
        auto area = juce::Rectangle<int> (0, 0, width, height).reduced (6, 0);

        if (auto* ownerView = getOwnerView())
            g.setColour (ownerView->findColour (juce::Label::textColourId));
        else
            g.setColour (juce::Colours::white);

        g.drawText (file.getFileName(), area, juce::Justification::centredLeft, true);
    }

    juce::String getTooltip() override
    {
        return file.getFullPathName();
    }

    int getId() const noexcept
    {
        return id;
    }

private:
    int id = 0;
    juce::File file;
};

} // namespace

class FileListRootItem : public juce::TreeViewItem
{
public:
    explicit FileListRootItem (MainComponent& ownerIn)
        : owner (ownerIn)
    {
        setOpen (true);
    }

    bool mightContainSubItems() override
    {
        return true;
    }

    juce::String getUniqueName() const override
    {
        return "file-list-root";
    }

    void refresh()
    {
        clearSubItems();

        for (const auto& entry : owner.fileEntries)
            addSubItem (new FileListItem (entry.id, entry.file));

        treeHasChanged();
    }

private:
    MainComponent& owner;
};

MainComponent::MainComponent()
{
    setSize (800, 600);

    fileListRootItem = std::make_unique<FileListRootItem> (*this);
    fileTreeView.setRootItem (fileListRootItem.get());
    fileTreeView.setRootItemVisible (false);
    fileTreeView.setDefaultOpenness (true);
    fileTreeView.setMultiSelectEnabled (true);
    fileTreeView.setOpenCloseButtonsVisible (false);
    fileTreeView.setColour (juce::TreeView::backgroundColourId,
                            findColour (juce::ResizableWindow::backgroundColourId));
    fileTreeView.setColour (juce::TreeView::selectedItemBackgroundColourId,
                            juce::Colours::cornflowerblue.withAlpha (0.45f));
    fileTreeBorder.setText ({ });
    fileTreeBorder.setText ("Measured Sweeps");
    fileTreeBorder.setColour (juce::GroupComponent::outlineColourId,
                              juce::Colours::grey);
    fileTreeBorder.setColour (juce::GroupComponent::textColourId,
                              juce::Colours::white);
    addAndMakeVisible (fileTreeBorder);
    addAndMakeVisible (fileTreeView);
    fileTreeBorder.toFront (false);

    addButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Open .wav files", juce::File(), "*.wav");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectMultipleItems
                              | juce::FileBrowserComponent::canSelectFiles,
                              [this, chooser] (const juce::FileChooser& result)
                              {
                                  addFiles (result.getResults());
                              });
    };

    removeButton.onClick = [this]
    {
        removeSelectedFiles();
    };

    addAndMakeVisible (addButton);
    addAndMakeVisible (removeButton);

    refreshFileList();
}

MainComponent::~MainComponent() = default;

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setFont (juce::FontOptions (20.0f));
    g.setColour (juce::Colours::white);
    g.drawText ("Convolver Batch WAV Convolution",
                getLocalBounds().reduced (12, 8), juce::Justification::topLeft, true);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced (12);

    bounds.removeFromTop (32);

    auto leftColumn = bounds.removeFromLeft (320);
    auto buttonRow = leftColumn.removeFromBottom (36);

    fileTreeBorder.setBounds (leftColumn);
    fileTreeView.setBounds (leftColumn.reduced (1).withTrimmedTop (18));

    auto addButtonBounds = buttonRow.removeFromLeft ((buttonRow.getWidth()) / 2);
    addButtonBounds.removeFromLeft(1);
    
    buttonRow.removeFromLeft (6);
    buttonRow.removeFromRight(1);

    addButton.setBounds (addButtonBounds);
    removeButton.setBounds (buttonRow);
}

void MainComponent::addFiles (const juce::Array<juce::File>& filesToAdd)
{
    for (const auto& file : filesToAdd)
    {
        if (! file.hasFileExtension ("wav"))
            continue;

        fileEntries.push_back ({ nextFileId++, file });
    }

    refreshFileList();
}

void MainComponent::removeSelectedFiles()
{
    std::unordered_set<int> selectedIds;

    for (int i = 0; i < fileTreeView.getNumSelectedItems(); ++i)
    {
        if (auto* item = dynamic_cast<FileListItem*> (fileTreeView.getSelectedItem (i)))
            selectedIds.insert (item->getId());
    }

    if (selectedIds.empty())
        return;

    std::erase_if (fileEntries, [&selectedIds] (const FileEntry& entry)
    {
        return selectedIds.find (entry.id) != selectedIds.end();
    });

    refreshFileList();
}

void MainComponent::refreshFileList()
{
    if (fileListRootItem != nullptr)
        fileListRootItem->refresh();
}
