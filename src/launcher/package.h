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

bool runtimeFilesReady(const Layout& layout);
bool audioCatalogComplete(const Layout& layout, std::wstring& detail);

// Checks required components without rehashing the whole package on every launch.
// An empty boot reference uses the documented NTSC-U ELF hash.
PackageReport inspectPackageForLaunch(const Layout& layout,
                                      std::wstring_view bootReference = {});

// Performs the full integrity audit, including every SHA-256 in the manifest.
PackageReport inspectPackage(const Layout& layout, std::wstring_view bootReference = {});

// Writes the package manifest only after all required components pass inspection.
bool writePackageManifest(const Layout& layout, std::wstring_view bootReference,
                          std::wstring& error);

} // namespace ds
