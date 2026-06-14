#pragma once
#include <juce_core/juce_core.h>
#include <atomic>

class EngineLauncher {
public:
    EngineLauncher();
    ~EngineLauncher();

    bool launch();
    void shutdown();
    bool isRunning() const;

    juce::File getEnginePath() const;
    void setEnginePath(const juce::File& path) { enginePath_ = path; }

private:
    juce::File enginePath_;
    std::unique_ptr<juce::ChildProcess> process_;
    std::atomic<bool> launched_{false};

    juce::File findDefaultEnginePath() const;
    juce::File findEngineBinary() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EngineLauncher)
};
