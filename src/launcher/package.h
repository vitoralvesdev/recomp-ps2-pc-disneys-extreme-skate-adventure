#pragma once

#include "layout.h"

namespace ds {

enum class CheckStatus { Ok, Warning, Error };

struct CheckItem {
    std::wstring label;
    CheckStatus status = CheckStatus::Ok;
    std::wstring detail;
};

struct PackageReport {
    std::vector<CheckItem> items;
    int errorCount = 0;
    int warningCount = 0;
    bool ready = false;
    std::wstring runtimeStamp;

    const CheckItem* find(std::wstring_view label) const;
};

// Checks the files the launcher needs before enabling "Jogar". Missing game data
// or runtime files are errors; missing optional prepared assets are warnings so
// the user can diagnose audio/visual problems without being blocked.
PackageReport inspectPackage(const Layout& layout);

} // namespace ds
