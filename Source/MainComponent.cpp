#include "MainComponent.h"

class FileListModel : public juce::ListBoxModel
{
public:
    explicit FileListModel (MainComponent& ownerIn)
        : owner (ownerIn)
    {
    }

    int getNumRows() override
    {
        return static_cast<int> (owner.fileEntries.size());
    }

    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (rowNumber, static_cast<int> (owner.fileEntries.size())))
            return;

        if (selected)
            g.fillAll (juce::Colours::cornflowerblue.withAlpha (0.45f));

        auto area = juce::Rectangle<int> (0, 0, width, height).reduced (6, 0);
        g.setColour (owner.fileListBox.findColour (juce::Label::textColourId));
        g.drawText (owner.fileEntries[(size_t) rowNumber].file.getFileName(),
                    area,
                    juce::Justification::centredLeft,
                    true);
    }

private:
    MainComponent& owner;
};

MainComponent::MainComponent()
{
    setSize (800, 600);

    fileListModel = std::make_unique<FileListModel> (*this);
    fileListBox.setModel (fileListModel.get());
    fileListBox.setMultipleSelectionEnabled (true);
    fileListBox.setRowHeight (24);
    fileListBox.setColour (juce::ListBox::backgroundColourId,
                           findColour (juce::ResizableWindow::backgroundColourId));
    fileListBorder.setText ("Convolve this files: ");
    fileListBorder.setColour (juce::GroupComponent::outlineColourId,
                              juce::Colours::grey);
    fileListBorder.setColour (juce::GroupComponent::textColourId,
                              juce::Colours::white);
    addAndMakeVisible (fileListBorder);
    addAndMakeVisible (fileListBox);
    fileListBorder.toFront (false);

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

    convolveFileBorder.setText ("With this file:");
    convolveFileBorder.setColour (juce::GroupComponent::outlineColourId,
                                  juce::Colours::grey);
    convolveFileBorder.setColour (juce::GroupComponent::textColourId,
                                  juce::Colours::white);
    addAndMakeVisible (convolveFileBorder);

    convolveFileLabel.setText ("(no file selected)", juce::dontSendNotification);
    convolveFileLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    convolveFileLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (convolveFileLabel);
    convolveFileBorder.toFront (false);

    convolveFileSelectButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Open .wav file", juce::File(), "*.wav");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                              [this, chooser] (const juce::FileChooser& result)
                              {
                                  auto results = result.getResults();
                                  if (! results.isEmpty())
                                  {
                                      convolveFile = results.getFirst();
                                      convolveFileLabel.setText (convolveFile.getFileName(),
                                                                 juce::dontSendNotification);
                                      convolveFileLabel.setTooltip (convolveFile.getFullPathName());
                                  }
                              });
    };
    addAndMakeVisible (convolveFileSelectButton);

    outputDirBorder.setText ("Output Directory:");
    outputDirBorder.setColour (juce::GroupComponent::outlineColourId,
                               juce::Colours::grey);
    outputDirBorder.setColour (juce::GroupComponent::textColourId,
                               juce::Colours::white);
    addAndMakeVisible (outputDirBorder);

    outputDirLabel.setText ("(no directory selected)", juce::dontSendNotification);
    outputDirLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    outputDirLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (outputDirLabel);
    outputDirBorder.toFront (false);

    outputDirSelectButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Select output directory");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
                              [this, chooser] (const juce::FileChooser& result)
                              {
                                  auto results = result.getResults();
                                  if (! results.isEmpty())
                                  {
                                      outputDirectory = results.getFirst();
                                      outputDirLabel.setText (outputDirectory.getFullPathName(),
                                                              juce::dontSendNotification);
                                      outputDirLabel.setTooltip (outputDirectory.getFullPathName());
                                  }
                              });
    };
    addAndMakeVisible (outputDirSelectButton);

    helpButton.onClick = [this]
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                "Help",
                                                "Select an impulse response, choose output directory, add WAV files and press Convolve.");
    };

    addAndMakeVisible (helpButton);
    addAndMakeVisible (convolveButton);

    refreshFileList();
}

MainComponent::~MainComponent()
{
    fileListBox.setModel (nullptr);
    fileListModel.reset();
}

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

    fileListBorder.setBounds (leftColumn);
    fileListBox.setBounds (leftColumn.reduced (1).withTrimmedTop (18));

    auto addButtonBounds = buttonRow.removeFromLeft ((buttonRow.getWidth()) / 2);
    addButtonBounds.removeFromLeft(1);

    buttonRow.removeFromLeft (6);
    buttonRow.removeFromRight(1);

    addButton.setBounds (addButtonBounds);
    removeButton.setBounds (buttonRow);

    auto rightColumn = bounds;
    rightColumn.removeFromLeft (12);

    constexpr int groupHeight = 48;
    constexpr int selectButtonWidth = 80;
    constexpr int spacing = 20;

    auto convolveFileRow = rightColumn.removeFromTop (groupHeight);
    auto convolveFileSelectArea = convolveFileRow.removeFromRight (selectButtonWidth);
    convolveFileBorder.setBounds (convolveFileRow);
    convolveFileLabel.setBounds (convolveFileRow.reduced (8).withTrimmedTop (14));
    convolveFileSelectButton.setBounds (convolveFileSelectArea.withTrimmedTop(6).withSizeKeepingCentre (selectButtonWidth, 36));

    rightColumn.removeFromTop (spacing);

    auto outputDirRow = rightColumn.removeFromTop (groupHeight);
    auto outputDirSelectArea = outputDirRow.removeFromRight (selectButtonWidth);
    outputDirBorder.setBounds (outputDirRow);
    outputDirLabel.setBounds (outputDirRow.reduced (8).withTrimmedTop (14));
    outputDirSelectButton.setBounds (outputDirSelectArea.withTrimmedTop(6).withSizeKeepingCentre(selectButtonWidth, 36));

    rightColumn.removeFromTop (spacing);

    // place convolve button snapped to bottom and help button just above it
    auto convolveArea = rightColumn.removeFromBottom (48);
    convolveButton.setBounds (convolveArea);

    auto helpArea = rightColumn.removeFromBottom (48);
    // inset help button slightly to match other controls height
    helpButton.setBounds (helpArea.withSizeKeepingCentre (convolveArea.getWidth(), 36));
}

void MainComponent::addFiles (const juce::Array<juce::File>& filesToAdd)
{
    for (const auto& file : filesToAdd)
    {
        if (file.hasFileExtension("wav"))
        {
            fileEntries.push_back ({ nextFileId++, file });
        }
    }

    refreshFileList();
}

void MainComponent::removeSelectedFiles()
{
    auto selectedRows = fileListBox.getSelectedRows();

    if (selectedRows.isEmpty())
        return;

    std::erase_if (fileEntries, [selectedRows, row = 0] (const FileEntry&) mutable
    {
        return selectedRows.contains (row++);
    });

    refreshFileList();
}

void MainComponent::refreshFileList()
{
    fileListBox.updateContent();
    fileListBox.repaint();
}
