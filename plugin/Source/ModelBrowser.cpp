#include "ModelBrowser.h"
#include "PluginProcessor.h"
#include "ClonadaLookAndFeel.h"

ModelBrowser::ModelBrowser(ClonadaProcessor& p) : processor_(p) {
    titleLabel_.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(ClonadaLookAndFeel::kCyanGlow));
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    closeButton_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(ClonadaLookAndFeel::kTextDim));
    closeButton_.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(closeButton_);

    modelList_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF0c0c14));
    modelList_.setColour(juce::ListBox::outlineColourId, juce::Colour(ClonadaLookAndFeel::kBorder));
    modelList_.setRowHeight(36);
    addAndMakeVisible(modelList_);

    dropLabel_.setFont(juce::FontOptions(11.0f));
    dropLabel_.setColour(juce::Label::textColourId, juce::Colour(ClonadaLookAndFeel::kTextDim));
    dropLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(dropLabel_);

    auto setupButton = [](juce::TextButton& btn, juce::uint32 col) {
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(col).withAlpha(0.15f));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colour(col));
    };

    setupButton(browseButton_, ClonadaLookAndFeel::kCyanGlow);
    browseButton_.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Models Folder", processor_.getModelsDirectory(), "");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.isDirectory()) {
                    processor_.setModelsDirectory(result);
                    refreshModelList();
                }
            });
    };
    addAndMakeVisible(browseButton_);

    setupButton(importButton_, ClonadaLookAndFeel::kCyanDim);
    importButton_.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Import Voice Model", juce::File(), "*.pth");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.existsAsFile())
                    importModel(result);
            });
    };
    addAndMakeVisible(importButton_);

    setupButton(trainButton_, ClonadaLookAndFeel::kAmber);
    trainButton_.onClick = [this] {
        auto& lic = processor_.getLicenseClient();
        if (!lic.isActivated()) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "License Required",
                "Voice training requires an active Clonada license.\nPlease activate your license first.");
            return;
        }

        if (!processor_.isEngineConnected()) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Engine Offline",
                "The cloud engine must be running to train voices.\nPlease wait for the engine to connect.");
            return;
        }

        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Training Audio (WAV/MP3)", juce::File(), "*.wav;*.mp3;*.flac");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto audioFile = fc.getResult();
                if (!audioFile.existsAsFile()) return;

                auto nameInput = std::make_shared<juce::AlertWindow>(
                    "Name Your Voice Model",
                    "Enter a name for this voice clone:\n\nFile: " + audioFile.getFileName(),
                    juce::MessageBoxIconType::QuestionIcon);
                nameInput->addTextEditor("name", audioFile.getFileNameWithoutExtension(), "Model Name:");
                nameInput->addButton("Train", 1);
                nameInput->addButton("Cancel", 0);

                nameInput->enterModalState(true, juce::ModalCallbackFunction::create(
                    [this, audioFile, nameInput](int btnResult) {
                        if (btnResult == 0) return;
                        auto modelName = nameInput->getTextEditorContents("name").trim();
                        if (modelName.isEmpty()) modelName = "voice_model";

                        trainButton_.setEnabled(false);
                        trainButton_.setButtonText("Training...");

                        juce::Thread::launch([this, audioPath = audioFile.getFullPathName(), modelName]() {
                            auto& bridge = processor_.getBridge();

                            auto escaped = audioPath.replace("\\", "\\\\").replace("\"", "\\\"");
                            juce::String jsonCmd = "{\"version\":\"1.0.0\",\"command\":\"TRAIN\",\"audio_path\":\""
                                + escaped + "\",\"model_name\":\"" + modelName + "\"}";

                            auto response = bridge.sendCommandSync(jsonCmd, 1800000);

                            juce::MessageManager::callAsync([this, response, modelName]() {
                                trainButton_.setEnabled(true);
                                trainButton_.setButtonText("Train New Voice");

                                if (response.contains("SUCCESS")) {
                                    refreshModelList();
                                    juce::AlertWindow::showMessageBoxAsync(
                                        juce::MessageBoxIconType::InfoIcon,
                                        "Training Complete",
                                        "Voice model \"" + modelName + "\" has been trained successfully!\n\n"
                                        "It's now available in your models list.");
                                } else {
                                    juce::String errorMsg = "Unknown error occurred during training.";
                                    if (response.contains("\"message\"")) {
                                        auto afterKey = response.fromFirstOccurrenceOf("\"message\"", false, true);
                                        auto afterColon = afterKey.fromFirstOccurrenceOf("\"", false, true);
                                        errorMsg = afterColon.upToFirstOccurrenceOf("\"", false, true);
                                    }
                                    juce::AlertWindow::showMessageBoxAsync(
                                        juce::MessageBoxIconType::WarningIcon,
                                        "Training Failed",
                                        "Could not train voice model:\n\n" + errorMsg);
                                }
                            });
                        });
                    }), true);
            });
    };
    addAndMakeVisible(trainButton_);

    refreshModelList();
}

ModelBrowser::~ModelBrowser() = default;

void ModelBrowser::refreshModelList() {
    models_.clear();
    auto dir = processor_.getModelsDirectory();

    if (!dir.isDirectory()) {
        auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
        dir = appData.getChildFile("Clonada").getChildFile("Models");
        dir.createDirectory();
        processor_.setModelsDirectory(dir);
    }

    for (const auto& file : dir.findChildFiles(juce::File::findFiles, false, "*.pth")) {
        models_.add({file.getFileNameWithoutExtension(), file, file.getSize()});
    }

    modelList_.updateContent();
    modelList_.repaint();
}

void ModelBrowser::importModel(const juce::File& file) {
    auto dest = processor_.getModelsDirectory().getChildFile(file.getFileName());
    if (file.copyFileTo(dest)) {
        refreshModelList();
    }
}

void ModelBrowser::loadSelectedModel() {
    if (selectedRow_ >= 0 && selectedRow_ < models_.size()) {
        processor_.setModelPath(models_[selectedRow_].file.getFullPathName());
        if (onModelSelected) onModelSelected();
    }
}

juce::String ModelBrowser::formatFileSize(int64_t bytes) const {
    if (bytes < 1024) return juce::String(bytes) + " B";
    if (bytes < 1048576) return juce::String(bytes / 1024) + " KB";
    return juce::String(bytes / 1048576) + " MB";
}

// FileDragAndDropTarget
bool ModelBrowser::isInterestedInFileDrag(const juce::StringArray& files) {
    for (auto& f : files)
        if (f.endsWithIgnoreCase(".pth")) return true;
    return false;
}

void ModelBrowser::filesDropped(const juce::StringArray& files, int, int) {
    dragHovering_ = false;
    for (auto& f : files) {
        juce::File file(f);
        if (file.hasFileExtension("pth"))
            importModel(file);
    }
    repaint();
}

void ModelBrowser::fileDragEnter(const juce::StringArray&, int, int) {
    dragHovering_ = true;
    repaint();
}

void ModelBrowser::fileDragExit(const juce::StringArray&) {
    dragHovering_ = false;
    repaint();
}

// ListBoxModel
int ModelBrowser::getNumRows() { return models_.size(); }

void ModelBrowser::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (row < 0 || row >= models_.size()) return;

    if (selected) {
        g.setColour(juce::Colour(ClonadaLookAndFeel::kCyanGlow).withAlpha(0.15f));
        g.fillRect(0, 0, w, h);
    }

    auto model = models_[row];

    // Model name
    g.setColour(juce::Colour(ClonadaLookAndFeel::kTextLight));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(model.name, 12, 0, w - 100, h, juce::Justification::centredLeft);

    // File size
    g.setColour(juce::Colour(ClonadaLookAndFeel::kTextDim));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText(formatFileSize(model.sizeBytes), w - 90, 0, 80, h, juce::Justification::centredRight);

    // Separator
    g.setColour(juce::Colour(ClonadaLookAndFeel::kBorder).withAlpha(0.3f));
    g.drawHorizontalLine(h - 1, 8.0f, (float)w - 8.0f);
}

void ModelBrowser::listBoxItemClicked(int row, const juce::MouseEvent&) {
    selectedRow_ = row;
}

void ModelBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent&) {
    selectedRow_ = row;
    loadSelectedModel();
}

void ModelBrowser::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // Panel background
    g.setColour(juce::Colour(ClonadaLookAndFeel::kPanel));
    g.fillRoundedRectangle(bounds, 10.0f);

    // Border
    auto borderCol = dragHovering_ ? juce::Colour(ClonadaLookAndFeel::kCyanGlow)
                                   : juce::Colour(ClonadaLookAndFeel::kBorder);
    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, dragHovering_ ? 2.0f : 1.0f);

    if (dragHovering_) {
        g.setColour(juce::Colour(ClonadaLookAndFeel::kCyanGlow).withAlpha(0.05f));
        g.fillRoundedRectangle(bounds.reduced(2.0f), 9.0f);
    }
}

void ModelBrowser::resized() {
    auto area = getLocalBounds().reduced(12);

    auto top = area.removeFromTop(28);
    titleLabel_.setBounds(top.removeFromLeft(200));
    closeButton_.setBounds(top.removeFromRight(28));

    area.removeFromTop(6);

    auto buttons = area.removeFromBottom(32);
    trainButton_.setBounds(buttons.removeFromRight(120));
    buttons.removeFromRight(6);
    importButton_.setBounds(buttons.removeFromRight(100));
    buttons.removeFromRight(6);
    browseButton_.setBounds(buttons.removeFromRight(120));

    area.removeFromBottom(6);
    dropLabel_.setBounds(area.removeFromBottom(20));
    area.removeFromBottom(4);

    modelList_.setBounds(area);
}
