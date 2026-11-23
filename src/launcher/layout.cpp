#include "layout.h"

namespace ds {

namespace {

bool isDirectory(const fs::path& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

} // namespace

Layout Layout::detect(const fs::path& exePath) {
    Layout layout;
    layout.exePath = exePath;
    layout.exeDir = exePath.parent_path();

    // Optional override used by portable installs on non-system drives and by
    // automated tests that must not touch the real user profile.
    wchar_t dataRootOverride[32768]{};
    const DWORD overrideLength =
        GetEnvironmentVariableW(L"DISNEY_DATA_ROOT", dataRootOverride, 32768);
    if (overrideLength > 0 && overrideLength < 32768) {
        layout.portable = false;
        layout.dataRoot = fs::path(dataRootOverride);
        layout.gameDir = layout.dataRoot / L"game";
        const fs::path localRuntime = layout.exeDir / L"runtime";
        layout.runtimeDir =
            isDirectory(localRuntime) ? localRuntime : layout.dataRoot / L"runtime";
        layout.assetsDir = layout.dataRoot / L"assets";
        layout.savesDir = layout.dataRoot / L"saves" / L"profile-1";
        layout.logsDir = layout.dataRoot / L"logs";
        layout.stateDir = layout.dataRoot / L"launcher";
        layout.configPath = layout.stateDir / L"config.ini";
        return layout;
    }

    layout.portable =
        isDirectory(layout.exeDir / L"original" / L"extracted") ||
        (fileExists(layout.exeDir / L"runtime" / L"DisneySkateRuntime.exe") &&
         isDirectory(layout.exeDir / L"assets" / L"native")) ||
        fileExists(layout.exeDir / L"launcher.portable");

    if (layout.portable) {
        layout.dataRoot = layout.exeDir;
        layout.gameDir = isDirectory(layout.exeDir / L"original" / L"extracted")
                             ? layout.exeDir / L"original" / L"extracted"
                             : layout.exeDir / L"game";
        layout.runtimeDir = layout.exeDir / L"runtime";
        layout.assetsDir = layout.exeDir / L"assets";
        layout.savesDir = layout.gameDir / L"mc0";
        layout.logsDir = layout.exeDir / L"analysis";
        layout.stateDir = layout.logsDir / L"launcher";
    } else {
        layout.dataRoot = localAppDataDirectory() / L"HighSky" / L"DisneySkate";
        layout.gameDir = layout.dataRoot / L"game";
        const fs::path localRuntime = layout.exeDir / L"runtime";
        layout.runtimeDir = isDirectory(localRuntime) ? localRuntime
                                                      : layout.dataRoot / L"runtime";
        layout.assetsDir = layout.dataRoot / L"assets";
        layout.savesDir = layout.dataRoot / L"saves" / L"profile-1";
        layout.logsDir = layout.dataRoot / L"logs";
        layout.stateDir = layout.dataRoot / L"launcher";
    }
    layout.configPath = layout.stateDir / L"config.ini";
    return layout;
}

fs::path Layout::gameElf() const { return gameDir / L"SLUS_206.07"; }
fs::path Layout::runtimeExe() const { return runtimeDir / L"DisneySkateRuntime.exe"; }
fs::path Layout::gameLog() const { return logsDir / L"game-session.log"; }
fs::path Layout::launcherLog() const { return logsDir / L"launcher.log"; }
fs::path Layout::installStatePath() const { return stateDir / L"install-state.ini"; }

bool Layout::hasRuntime() const { return fileExists(runtimeExe()); }

bool Layout::hasGameData() const {
    return fileExists(gameElf()) && fileExists(gameDir / L"SKATE4.HED") &&
           fileExists(gameDir / L"SKATE4.WAD");
}

bool Layout::hasAssets() const { return isDirectory(assetsDir / L"native"); }

bool Layout::ensureUserDirectories(std::wstring& error) const {
    return ensureDirectory(dataRoot, error) && ensureDirectory(gameDir, error) &&
           ensureDirectory(assetsDir, error) && ensureDirectory(runtimeDir, error) &&
           ensureDirectory(savesDir, error) && ensureDirectory(logsDir, error) &&
           ensureDirectory(stateDir, error);
}

std::wstring Layout::describe() const {
    return std::wstring(L"Modo: ") + (portable ? L"portátil (repositório)" : L"instalado") +
           L"\nDados: " + dataRoot.wstring() + L"\nJogo: " + gameDir.wstring();
}

} // namespace ds
