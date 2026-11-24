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
// exact SYSTEM.CNF boot entry, VMODE, ELF header and reference SHA-256 of
// SLUS_206.07, plus non-empty required data modules. The full ISO is not hashed.
bool validateIso(IsoImage& image, IsoValidation& validation);

bool sha256IsoEntry(IsoImage& image, const IsoEntry& entry, std::wstring& digest,
                    std::wstring& error);

const wchar_t* expectedSerial();
const wchar_t* expectedBootSha256();

} // namespace ds
