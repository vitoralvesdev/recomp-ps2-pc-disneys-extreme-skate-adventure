#pragma once

// Single source of truth for the launcher version. launcher.rc uses the numeric
// fields; the strings are shown in the UI and recorded in install-state.ini.
#define DISNEY_LAUNCHER_VERSION_STRING L"0.3.0"
#define DISNEY_LAUNCHER_VERSION_MAJOR 0
#define DISNEY_LAUNCHER_VERSION_MINOR 3
#define DISNEY_LAUNCHER_VERSION_PATCH 0
#define DISNEY_LAUNCHER_VERSION_BUILD 0

#define DISNEY_LAUNCHER_VERSION_STRING_UTF8 "0.3.0"

// Default update source. A fork can override this in config.ini in the future;
// for now the official repository is the only accepted origin.
#define DISNEY_LAUNCHER_REPOSITORY_OWNER L"vitoralvesdev"
#define DISNEY_LAUNCHER_REPOSITORY_NAME L"recomp-ps2-pc-disneys-extreme-skate-adventure"
