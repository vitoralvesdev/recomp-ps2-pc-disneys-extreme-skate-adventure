#pragma once

#include "common.h"

namespace ds {

// Resolves where the launcher, game data, runtime and user data live.
//
// Portable/developer layout (the repository checkout):
//   <root>\runtime, <root>\assets, <root>\original\extracted
//
// Installed layout (the launcher lives next to its runtime, user data lives in
// %LOCALAPPDATA%\HighSky\DisneySkate so updates never touch saves):
//   <app>\runtime, <app>\Disney's Extreme Skate Adventure.exe
//   %LOCALAPPDATA%\HighSky\DisneySkate\{game,assets,saves,logs,launcher}
struct Layout {
    fs::path exePath;
    fs::path exeDir;
    fs::path dataRoot;
    fs::path gameDir;
    fs::path runtimeDir;
    fs::path assetsDir;
    fs::path savesDir;
    fs::path logsDir;
    fs::path stateDir;
    fs::path configPath;
    bool portable = false;

    static Layout detect(const fs::path& exePath);

    fs::path gameElf() const;
    fs::path runtimeExe() const;
    fs::path gameLog() const;
    fs::path launcherLog() const;
    fs::path installStatePath() const;

    bool hasRuntime() const;
    bool hasGameData() const;
    bool hasAssets() const;
    bool ensureUserDirectories(std::wstring& error) const;

    std::wstring describe() const;
};

} // namespace ds
