#include "MainComponent.h"
#include "DarkLookAndFeel.h"
#include "ConvolutionEngine.h"

class ConvolveButtonLookAndFeel : public DarkLookAndFeel
{
public:
    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        auto cornerSize = juce::jmin (bounds.getHeight() * 0.5f, 6.0f);

        const bool isProcessing = button.getProperties().getWithDefault ("convolveProcessing", false);
        const float progress = juce::jlimit (0.0f, 1.0f, (float) button.getProperties().getWithDefault ("convolveProgress", 0.0f));

        auto baseColour = backgroundColour;
        auto highlightColour = backgroundColour.interpolatedWith (juce::Colours::white, 0.42f);

        g.setColour (baseColour);
        g.fillRoundedRectangle (bounds, cornerSize);

        if (isProcessing && progress > 0.0f)
        {
            auto progressBounds = bounds;
            progressBounds.setWidth (bounds.getWidth() * progress);

            g.setColour (highlightColour);
            g.fillRoundedRectangle (progressBounds, cornerSize);
        }

        if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
        {
            g.setColour (juce::Colours::white.withAlpha (shouldDrawButtonAsDown ? 0.14f : 0.08f));
            g.fillRoundedRectangle (bounds, cornerSize);
        }

        g.setColour (button.findColour (juce::GroupComponent::outlineColourId));
        g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        g.setColour (button.findColour (juce::TextButton::textColourOffId));
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
    }
};

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
            g.fillAll (owner.findColour (juce::TextButton::buttonColourId).withAlpha (0.45f));

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
    setSize (900, 495);

    // Use a dedicated LookAndFeel for dark mode
    darkLookAndFeel = std::make_unique<DarkLookAndFeel>();
    setLookAndFeel (darkLookAndFeel.get());

    convolveButtonLookAndFeel = std::make_unique<ConvolveButtonLookAndFeel>();

    fileListModel = std::make_unique<FileListModel> (*this);
    fileListBox.setModel (fileListModel.get());
    fileListBox.setMultipleSelectionEnabled (true);
    fileListBox.setRowHeight (24);
    fileListBox.setColour (juce::ListBox::backgroundColourId,
                           findColour (juce::ListBox::backgroundColourId));
    fileListBorder.setText ("Convolve these files: ");
    fileListBorder.setColour (juce::GroupComponent::outlineColourId,
                              findColour (juce::GroupComponent::outlineColourId));
    fileListBorder.setColour (juce::GroupComponent::textColourId,
                              findColour (juce::GroupComponent::textColourId));
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
                                  findColour (juce::GroupComponent::outlineColourId));
    convolveFileBorder.setColour (juce::GroupComponent::textColourId,
                                  findColour (juce::GroupComponent::textColourId));
    addAndMakeVisible (convolveFileBorder);

    convolveFileLabel.setText ("(no file selected)", juce::dontSendNotification);
    convolveFileLabel.setColour (juce::Label::textColourId, findColour (juce::Label::textColourId));
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

    outputDirBorder.setText ("Output Path:");
    outputDirBorder.setColour (juce::GroupComponent::outlineColourId,
                               findColour (juce::GroupComponent::outlineColourId));
    outputDirBorder.setColour (juce::GroupComponent::textColourId,
                               findColour (juce::GroupComponent::textColourId));
    addAndMakeVisible (outputDirBorder);

    outputDirLabel.setText ("(no directory selected)", juce::dontSendNotification);
    outputDirLabel.setColour (juce::Label::textColourId, findColour (juce::Label::textColourId));
    outputDirLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (outputDirLabel);
    outputDirBorder.toFront (false);

    outputDirSelectButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Select output path");

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

    prefixBorder.setText ("Output Prefix:");
    prefixBorder.setColour (juce::GroupComponent::outlineColourId,
                            findColour (juce::GroupComponent::outlineColourId));
    prefixBorder.setColour (juce::GroupComponent::textColourId,
                            findColour (juce::GroupComponent::textColourId));
    addAndMakeVisible (prefixBorder);

    prefixEditor.setTextToShowWhenEmpty ("e.g. convolved_", juce::Colours::grey);
    prefixEditor.setColour (juce::TextEditor::backgroundColourId,
                            findColour (juce::ListBox::backgroundColourId));
    prefixEditor.setColour (juce::TextEditor::textColourId,
                            findColour (juce::Label::textColourId));
    prefixEditor.setColour (juce::TextEditor::outlineColourId,
                            findColour (juce::GroupComponent::outlineColourId));
    addAndMakeVisible (prefixEditor);
    prefixBorder.toFront (false);

    maintainLevelToggle.setToggleState (true, juce::dontSendNotification);
    maintainLevelToggle.setColour (juce::ToggleButton::textColourId,
                                   findColour (juce::Label::textColourId));
    maintainLevelToggle.setColour (juce::ToggleButton::tickColourId,
                                   findColour (juce::Label::textColourId));
    addAndMakeVisible (maintainLevelToggle);

    helpButton.onClick = [this]
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                "Help",
                                                "Select an impulse response, choose output directory, set a filename prefix, add WAV files and press Convolve.");
    };

    convolveButton.onClick = [this]
    {
        if (fileEntries.empty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Error", "No WAV files added.");
            return;
        }

        if (! convolveFile.existsAsFile())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Error", "No impulse response file selected.");
            return;
        }

        if (! outputDirectory.isDirectory())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Error", "No output directory selected.");
            return;
        }

        auto prefix = prefixEditor.getText();

        std::vector<ConvolutionEngine::FilePair> filePairs;
        filePairs.reserve (fileEntries.size());

        for (const auto& entry : fileEntries)
        {
            auto outputFileName = prefix + entry.file.getFileName();
            filePairs.push_back ({ entry.file,
                                   outputDirectory.getChildFile (outputFileName) });
        }

        startConvolutionBatch (std::move (filePairs));
    };

    addAndMakeVisible (helpButton);
    addAndMakeVisible (convolveButton);
    convolveButton.setLookAndFeel (convolveButtonLookAndFeel.get());
    convolveButton.getProperties().set ("convolveProcessing", false);
    convolveButton.getProperties().set ("convolveProgress", 0.0f);

    refreshFileList();
}

MainComponent::~MainComponent()
{
    if (convolveWorker.joinable())
        convolveWorker.join();

    convolveButton.setLookAndFeel (nullptr);
    fileListBox.setModel (nullptr);
    fileListModel.reset();
    setLookAndFeel (nullptr);
    darkLookAndFeel.reset();
    convolveButtonLookAndFeel.reset();
}

void MainComponent::setConvolveProgress (float progress)
{
    auto clampedProgress = juce::jlimit (0.0f, 1.0f, progress);
    convolveButton.getProperties().set ("convolveProgress", clampedProgress);
    convolveButton.repaint();
}

void MainComponent::startConvolutionBatch (std::vector<ConvolutionEngine::FilePair> filePairs)
{
    if (convolving.exchange (true))
        return;

    convolveButton.setEnabled (false);
    convolveButton.getProperties().set ("convolveProcessing", true);
    setConvolveProgress (0.0f);

    auto safeThis = juce::Component::SafePointer<MainComponent> (this);
    auto irFile = convolveFile;
    auto maintainLevels = maintainLevelToggle.getToggleState();

    convolveWorker = std::jthread ([safeThis,
                                    filePairs = std::move (filePairs),
                                    irFile,
                                    maintainLevels] (std::stop_token)
    {
        if (safeThis == nullptr)
            return;

        juce::StringArray errors;

        auto progressCallback = [safeThis] (double progress)
        {
            if (safeThis == nullptr)
                return;

            juce::MessageManager::callAsync ([safeThis, progress]
            {
                if (safeThis == nullptr)
                    return;

                safeThis->setConvolveProgress ((float) progress);
            });
        };

        auto successCount = ConvolutionEngine::processBatch (filePairs, irFile,
                                                             maintainLevels, errors,
                                                             progressCallback);

        juce::String message = juce::String (successCount) + " of "
                             + juce::String (static_cast<int> (filePairs.size()))
                             + " files convolved successfully.";

        if (! errors.isEmpty())
            message += "\n\nErrors:\n" + errors.joinIntoString ("\n");

        auto icon = errors.isEmpty() ? juce::AlertWindow::InfoIcon
                                     : juce::AlertWindow::WarningIcon;

        juce::MessageManager::callAsync ([safeThis, message, icon]
        {
            if (safeThis == nullptr)
                return;

            safeThis->convolving.store (false);
            safeThis->convolveButton.getProperties().set ("convolveProcessing", false);
            safeThis->setConvolveProgress (0.0f);
            safeThis->convolveButton.setEnabled (true);

            juce::AlertWindow::showMessageBoxAsync (icon, "Convolution Complete", message);
        });
    });
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setFont (juce::FontOptions (20.0f));
    g.setColour (findColour (juce::Label::textColourId));
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

    auto prefixRow = rightColumn.removeFromTop (groupHeight).removeFromLeft (380);
    prefixBorder.setBounds (prefixRow);
    prefixEditor.setBounds (prefixRow.reduced (8).withTrimmedTop (10));

    rightColumn.removeFromTop (spacing);

    auto toggleRow = rightColumn.removeFromTop (24);
    maintainLevelToggle.setBounds (toggleRow);

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
