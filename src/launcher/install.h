#pragma once

#include "config.h"
#include "layout.h"
#include "validate.h"

namespace ds {

enum class InstallStep { Validate = 0, Extract, Assets, Runtime, Verify, Count };

const wchar_t* installStepName(InstallStep step);
int installStepWeight(InstallStep step);

enum class StepState { Pending, Running, Done, Warning, Failed, Skipped };

struct InstallEvent {
    InstallStep step = InstallStep::Validate;
    StepState state = StepState::Pending;
    int stepPercent = 0;
    int overallPercent = 0;
    std::wstring message;
    std::wstring logLine;
};

struct InstallOptions {
    fs::path isoPath;
    bool forceExtract = false;
};

struct InstallResult {
    bool ok = false;
    bool cancelled = false;
    std::wstring message;
    IsoValidation validation;
};

// Runs the complete first-install pipeline. Steps already satisfied by an
// existing package are skipped, so a port update never asks for the ISO again.
InstallResult performInstall(const Layout& layout, Settings& settings,
                             const InstallOptions& options, const CancelToken& cancel,
                             const std::function<void(const InstallEvent&)>& onEvent);

// True when every file the ISO would write is already present with the expected
// size. Used for fast revalidation and for skipping extraction.
bool gameDataMatchesIso(const Layout& layout, IsoImage& image, std::wstring& detail);

fs::path preparationSourceRoot(const Layout& layout, const Settings& settings);

} // namespace ds
