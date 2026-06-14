#include "EngineLauncher.h"

EngineLauncher::EngineLauncher() {
    enginePath_ = findDefaultEnginePath();
}

EngineLauncher::~EngineLauncher() {
    shutdown();
}

juce::File EngineLauncher::findDefaultEnginePath() const {
    juce::StringArray searchPaths;

#if JUCE_MAC
    searchPaths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                        .getChildFile("Clonada/python").getFullPathName());
    searchPaths.add("~/Library/Application Support/Clonada/engine");
#elif JUCE_WINDOWS
    auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    searchPaths.add(home.getChildFile("Clonada/python").getFullPathName());
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    searchPaths.add(appData.getChildFile("Clonada/engine").getFullPathName());
    searchPaths.add("C:/Program Files/Clonada/engine");
#else
    searchPaths.add("~/.local/share/Clonada/engine");
    searchPaths.add("/opt/clonada/engine");
#endif

    for (auto& path : searchPaths) {
        auto dir = juce::File(path);
        if (dir.getChildFile("clonada_server.py").existsAsFile())
            return dir;
    }

    return {};
}

juce::File EngineLauncher::findEngineBinary() const {
#if JUCE_MAC
    juce::StringArray binaryPaths = {
        "/usr/local/bin/clonada-engine",
        "/opt/homebrew/bin/clonada-engine"
    };
    auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    binaryPaths.add(home.getChildFile("Clonada/clonada-engine").getFullPathName());
#elif JUCE_WINDOWS
    auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    juce::StringArray binaryPaths = {
        "C:/Program Files/Clonada/clonada-engine.exe",
        home.getChildFile("Clonada/clonada-engine.exe").getFullPathName()
    };
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    binaryPaths.add(appData.getChildFile("Clonada/clonada-engine.exe").getFullPathName());
#else
    juce::StringArray binaryPaths = {
        "/usr/local/bin/clonada-engine",
        "/opt/clonada/clonada-engine"
    };
#endif

    for (auto& path : binaryPaths) {
        auto f = juce::File(path);
        if (f.existsAsFile())
            return f;
    }
    return {};
}

juce::File EngineLauncher::getEnginePath() const {
    return enginePath_;
}

bool EngineLauncher::launch() {
    if (launched_.load() && isRunning())
        return true;

    // Try compiled binary first (installed by PKG/EXE installer)
    auto engineBinary = findEngineBinary();
    if (engineBinary.existsAsFile()) {
        process_ = std::make_unique<juce::ChildProcess>();
        juce::StringArray args;
        args.add(engineBinary.getFullPathName());
        args.add("--port");
        args.add("5050");

        if (process_->start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr)) {
            launched_.store(true);
            juce::Thread::sleep(500);
            return true;
        }
        process_.reset();
    }

    // Fall back to Python script
    if (!enginePath_.isDirectory())
        return false;

    auto script = enginePath_.getChildFile("clonada_server.py");
    if (!script.existsAsFile())
        return false;

    juce::File pythonExe;
    auto clonadaHome = enginePath_.getParentDirectory();

#if JUCE_WINDOWS
    auto condaPython = clonadaHome.getChildFile("miniconda/envs/clonada/python.exe");
    if (condaPython.existsAsFile())
        pythonExe = condaPython;
    else {
        auto bundledPython = clonadaHome.getChildFile("python/python.exe");
        if (bundledPython.existsAsFile())
            pythonExe = bundledPython;
        else
            pythonExe = juce::File("python");
    }
#else
    auto condaPython = clonadaHome.getChildFile("miniconda/envs/clonada/bin/python3");
    if (condaPython.existsAsFile())
        pythonExe = condaPython;
    else {
        auto bundledPython = clonadaHome.getChildFile("python/bin/python3");
        if (bundledPython.existsAsFile())
            pythonExe = bundledPython;
        else
            pythonExe = juce::File("/usr/bin/python3");
    }
#endif

    process_ = std::make_unique<juce::ChildProcess>();
    juce::StringArray args;
    args.add(pythonExe.getFullPathName());
    args.add(script.getFullPathName());
    args.add("--port");
    args.add("5050");

    if (process_->start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr)) {
        launched_.store(true);
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
