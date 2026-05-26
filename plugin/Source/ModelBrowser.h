#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class ClonadaProcessor;

class ModelBrowser : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     public juce::ListBoxModel {
public:
    explicit ModelBrowser(ClonadaProcessor& processor);
    ~ModelBrowser() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    std::function<void()> onModelSelected;
    std::function<void()> onClose;

private:
    void refreshModelList();
    void loadSelectedModel();
    void importModel(const juce::File& file);
    juce::String formatFileSize(int64_t bytes) const;

    ClonadaProcessor& processor_;

    juce::ListBox modelList_{"Models", this};
    juce::TextButton browseButton_{"Browse Folder..."};
    juce::TextButton importButton_{"Import .pth"};
    juce::TextButton trainButton_{"Train New Voice"};
    juce::TextButton closeButton_{"X"};
    juce::Label titleLabel_{"", "VOICE MODELS"};
    juce::Label dropLabel_{"", "Drop .pth files here"};

    struct ModelInfo {
        juce::String name;
        juce::File file;
        int64_t sizeBytes;
    };
    juce::Array<ModelInfo> models_;

    int selectedRow_ = -1;
    bool dragHovering_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModelBrowser)
};
