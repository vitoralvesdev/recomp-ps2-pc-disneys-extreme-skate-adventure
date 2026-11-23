#pragma once

#include "iso.h"

namespace ds {

struct IsoValidation {
    bool ok = false;
    std::wstring serial = L"SLUS-20607";
    std::wstring bootPath;
    std::wstring version;
    std::wstring vmode;
    std::wstring volumeLabel;
    std::wstring bootSha256;
    uint64_t imageBytes = 0;
    uint64_t payloadBytes = 0;
    size_t fileCount = 0;
    std::vector<std::wstring> errors;
    std::vector<std::wstring> warnings;

    std::wstring summary() const;
};

// Confirms that the image is the expected NTSC-U revision before writing data:
// SYSTEM.CNF boot entry, VMODE, ELF header of SLUS_206.07 and the expected data
// modules. It does not hash the entire image (multi-gigabyte); the boot ELF hash
// is recorded in the installation state instead.
bool validateIso(IsoImage& image, IsoValidation& validation);

const wchar_t* expectedSerial();

} // namespace ds
