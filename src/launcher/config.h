#pragma once

#include "common.h"

namespace ds {

struct Settings {
    std::wstring lastIsoPath;
    int renderScale = 1; // PS2X_GS_HW_SCALE: 1x, 2x or 3x
    bool gpuRenderer = true;
    bool nativeVblank = true;
    bool hangDiagnostics = false;
    bool checkUpdatesOnStart = true;
    std::wstring sourceRoot; // optional source checkout used for preparation/build steps
};

class Config {
public:
    // A missing file is not an error: defaults are used.
    bool load(const fs::path& path, std::wstring& error);
    bool save(const fs::path& path, std::wstring& error) const;

    Settings& settings() { return settings_; }
    const Settings& settings() const { return settings_; }
    fs::path path() const { return path_; }

private:
    Settings settings_;
    fs::path path_;
};

const wchar_t* renderScaleLabel(int scale);

} // namespace ds
