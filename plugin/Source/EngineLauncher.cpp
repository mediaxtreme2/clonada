#include "EngineLauncher.h"

EngineLauncher::EngineLauncher() {
    enginePath_ = findDefaultEnginePath();
}

EngineLauncher::~EngineLauncher() {
    shutdown();
}

juce::File EngineLauncher::findDefaultEnginePath() const {
    // Look for the Python engine relative to the plugin location
    // Installation layout: <install_dir>/engine/clonada_engine.py
    // Or system-wide: ~/Clonada/engine/clonada_engine.py

    juce::StringArray searchPaths;

#if JUCE_MAC
    searchPaths.add("~/Library/Application Support/Clonada/engine");
    searchPaths.add("/Applications/Clonada/engine");
#elif JUCE_WINDOWS
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    searchPaths.add(appData.getChildFile("Clonada/engine").getFullPathName());
    searchPaths.add("C:/Program Files/Clonada/engine");
#else
    searchPaths.add("~/.local/share/Clonada/engine");
    searchPaths.add("/opt/clonada/engine");
#endif

    for (auto& path : searchPaths) {
        auto dir = juce::File(path);
        auto script = dir.getChildFile("clonada_engine.py");
        if (script.existsAsFile())
            return dir;
    }

    return {};
}

juce::File EngineLauncher::getEnginePath() const {
    return enginePath_;
}

bool EngineLauncher::launch() {
    if (launched_.load() && isRunning())
        return true;

    if (!enginePath_.isDirectory())
        return false;

    auto script = enginePath_.getChildFile("clonada_engine.py");
    if (!script.existsAsFile())
        return false;

    // Look for bundled Python (Miniconda) or system Python
    juce::File pythonExe;

#if JUCE_WINDOWS
    auto bundledPython = enginePath_.getParentDirectory().getChildFile("python/python.exe");
    if (bundledPython.existsAsFile())
        pythonExe = bundledPython;
    else
        pythonExe = juce::File("python");
#else
    auto bundledPython = enginePath_.getParentDirectory().getChildFile("python/bin/python3");
    if (bundledPython.existsAsFile())
        pythonExe = bundledPython;
    else
        pythonExe = juce::File("/usr/bin/python3");
#endif

    process_ = std::make_unique<juce::ChildProcess>();
    juce::StringArray args;
    args.add(pythonExe.getFullPathName());
    args.add(script.getFullPathName());
    args.add("--port");
    args.add("5050");

    if (process_->start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr)) {
        launched_.store(true);
        // Give the engine a moment to start listening
        juce::Thread::sleep(500);
        return true;
    }

    process_.reset();
    return false;
}

void EngineLauncher::shutdown() {
    if (process_) {
        process_->kill();
        process_.reset();
    }
    launched_.store(false);
}

bool EngineLauncher::isRunning() const {
    return process_ && process_->isRunning();
}
