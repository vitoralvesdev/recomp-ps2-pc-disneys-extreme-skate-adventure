#include "validate.h"

#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cwctype>
#include <sstream>

namespace ds {
namespace {

constexpr const wchar_t* kSerial = L"SLUS-20607";
constexpr const wchar_t* kBootFile = L"SLUS_206.07";

std::wstring upperCopy(std::wstring_view text) {
    std::wstring result(text);
    for (wchar_t& ch : result) ch = static_cast<wchar_t>(std::towupper(ch));
    return result;
}

std::vector<std::wstring> splitLines(std::wstring_view text) {
    std::vector<std::wstring> lines;
    std::wstring current;
    for (wchar_t ch : text) {
        if (ch == L'\r') continue;
        if (ch == L'\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

std::wstring valueAfterEquals(std::wstring_view line) {
    const size_t equals = line.find(L'=');
    if (equals == std::wstring_view::npos) return {};
    std::wstring value = trim(line.substr(equals + 1));
    if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

bool sha256IsoEntry(IsoImage& image, const IsoEntry& entry, std::wstring& digest,
                    std::wstring& error) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<uint8_t> object;
    std::array<uint8_t, 32> result{};
    DWORD objectSize = 0;
    DWORD resultSize = 0;
    DWORD received = 0;
    bool ok = false;
    do {
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
            error = L"Não foi possível iniciar o SHA-256.";
            break;
        }
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                              reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &received,
                              0) < 0 ||
            BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&resultSize),
                              sizeof(resultSize), &received, 0) < 0 ||
            resultSize != result.size()) {
            error = L"Não foi possível consultar o SHA-256.";
            break;
        }
        object.resize(objectSize);
        if (BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) < 0) {
            error = L"Não foi possível criar o hash.";
            break;
        }
        std::vector<uint8_t> buffer(1 << 20);
        uint64_t offset = 0;
        while (offset < entry.size) {
            const size_t chunk = static_cast<size_t>(std::min<uint64_t>(buffer.size(), entry.size - offset));
            if (!image.readRegion(entry, offset, buffer.data(), chunk, error)) break;
            if (BCryptHashData(hash, buffer.data(), static_cast<ULONG>(chunk), 0) < 0) {
                error = L"Falha ao calcular o hash do ELF.";
                break;
            }
            offset += chunk;
        }
        if (!error.empty()) break;
        if (BCryptFinishHash(hash, result.data(), resultSize, 0) < 0) {
            error = L"Falha ao concluir o hash do ELF.";
            break;
        }
        static const wchar_t digits[] = L"0123456789abcdef";
        digest.clear();
        for (uint8_t byte : result) {
            digest.push_back(digits[byte >> 4]);
            digest.push_back(digits[byte & 0x0f]);
        }
        ok = true;
    } while (false);
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
}

} // namespace

const wchar_t* expectedSerial() { return kSerial; }

std::wstring IsoValidation::summary() const {
    if (!ok) {
        if (errors.empty()) return L"A imagem não foi validada.";
        return errors.front();
    }
    std::wstring text = L"ISO NTSC-U validada: " + serial;
    if (!version.empty()) text += L" (versão " + version + L")";
    text += L"\n" + formatBytes(payloadBytes) + L" em " + std::to_wstring(fileCount) +
            L" arquivos";
    if (!bootSha256.empty()) text += L"\nSHA-256 do ELF: " + bootSha256;
    return text;
}

bool validateIso(IsoImage& image, IsoValidation& validation) {
    validation = IsoValidation{};
    validation.serial = kSerial;
    validation.imageBytes = image.imageBytes();
    validation.payloadBytes = image.payloadBytes();
    validation.volumeLabel = image.volumeLabel();
    validation.fileCount = 0;
    for (const IsoEntry& entry : image.entries()) {
        if (!entry.directory) ++validation.fileCount;
    }

    if (!image.isOpen()) {
        validation.errors.push_back(L"A imagem não foi aberta.");
        return false;
    }

    const IsoEntry* systemCnf = image.find(L"SYSTEM.CNF");
    if (!systemCnf) {
        validation.errors.push_back(
            L"SYSTEM.CNF não foi encontrado. Esta imagem não parece ser o disco de "
            L"PlayStation 2 do jogo.");
        return false;
    }
    std::vector<uint8_t> cnfBytes;
    std::wstring error;
    if (!image.readAll(*systemCnf, cnfBytes, 4096, error)) {
        validation.errors.push_back(L"Não foi possível ler SYSTEM.CNF: " + error);
        return false;
    }
    const std::wstring cnfText = toWideLenient(std::string(cnfBytes.begin(), cnfBytes.end()));
    for (const std::wstring& rawLine : splitLines(cnfText)) {
        const std::wstring line = trim(rawLine);
        if (line.empty()) continue;
        const std::wstring key = upperCopy(trim(line.substr(0, line.find(L'='))));
        if (key == L"BOOT2") {
            validation.bootPath = valueAfterEquals(line);
        } else if (key == L"VER") {
            validation.version = valueAfterEquals(line);
        } else if (key == L"VMODE") {
            validation.vmode = valueAfterEquals(line);
        }
    }
    if (validation.bootPath.empty()) {
        validation.errors.push_back(L"SYSTEM.CNF não declara BOOT2; a imagem está incompleta.");
    } else if (validation.bootPath.find(kBootFile) == std::wstring::npos) {
        validation.errors.push_back(
            L"Esta imagem não é a versão esperada. BOOT2 aponta para " +
            validation.bootPath + L", mas o projeto exige " + kBootFile + L" (NTSC-U).");
    }
    if (!validation.vmode.empty() && upperCopy(validation.vmode) != L"NTSC") {
        validation.errors.push_back(
            L"Região não suportada: VMODE = " + validation.vmode +
            L". Use a edição americana (NTSC-U, " + kBootFile + L").");
    } else if (validation.vmode.empty()) {
        validation.warnings.push_back(L"SYSTEM.CNF não informa VMODE; a serial será conferida.");
    }

    const IsoEntry* boot = image.find(kBootFile);
    if (!boot) {
        validation.errors.push_back(L"O executável " + std::wstring(kBootFile) +
                                    L" não foi encontrado na imagem.");
    } else {
        if (boot->size < 1024 * 1024) {
            validation.errors.push_back(L"O executável do jogo tem um tamanho inesperado.");
        }
        std::array<uint8_t, 64> header{};
        if (!image.readRegion(*boot, 0, header.data(), header.size(), error)) {
            validation.errors.push_back(L"Não foi possível ler o executável do jogo: " + error);
        } else {
            if (std::memcmp(header.data(), "\x7f" "ELF", 4) != 0) {
                validation.errors.push_back(L"O executável do jogo não é um ELF válido.");
            } else {
                if (header[4] != 1) validation.errors.push_back(L"ELF não é de 32 bits.");
                if (header[5] != 1) validation.errors.push_back(L"ELF não é little-endian.");
                const uint16_t machine = static_cast<uint16_t>(header[18] | (header[19] << 8));
                const uint16_t type = static_cast<uint16_t>(header[16] | (header[17] << 8));
                if (machine != 8) validation.errors.push_back(L"ELF não é MIPS (R5900).");
                if (type != 2) validation.errors.push_back(L"ELF não é um executável.");
                const uint32_t entryPoint = static_cast<uint32_t>(header[24]) |
                                            (static_cast<uint32_t>(header[25]) << 8) |
                                            (static_cast<uint32_t>(header[26]) << 16) |
                                            (static_cast<uint32_t>(header[27]) << 24);
                if (entryPoint == 0) validation.errors.push_back(L"ELF sem ponto de entrada.");
            }
        }
        std::wstring hashError;
        if (!sha256IsoEntry(image, *boot, validation.bootSha256, hashError)) {
            validation.warnings.push_back(L"Não foi possível calcular o hash do ELF: " + hashError);
        }
    }

    struct RequiredFile {
        const wchar_t* path;
        uint64_t minimumBytes;
    };
    static constexpr RequiredFile required[] = {
        {L"SKATE4.HED", 1024},
        {L"SKATE4.WAD", 10ull * 1024 * 1024},
        {L"IOP/SIO2MAN.IRX", 1024},
        {L"IOP/PADMAN.IRX", 1024},
        {L"IOP/LIBSD.IRX", 1024},
        {L"IOP/SDRDRV.IRX", 1024},
        {L"IOP/MCMAN.IRX", 1024},
        {L"IOP/MCSERV.IRX", 1024},
        {L"IOP/EZPCM.IRX", 1024},
    };
    for (const RequiredFile& file : required) {
        const IsoEntry* entry = image.find(file.path);
        if (!entry) {
            validation.errors.push_back(L"Arquivo obrigatório ausente na imagem: " +
                                        std::wstring(file.path));
        } else if (entry->size < file.minimumBytes) {
            validation.errors.push_back(L"Arquivo truncado na imagem: " +
                                        std::wstring(file.path));
        }
    }
    if (!image.find(L"IOP/IOPRP270.IMG")) {
        validation.warnings.push_back(
            L"IOPRP270.IMG não está presente; o runtime usa os módulos IOP individuais.");
    }

    validation.ok = validation.errors.empty();
    return validation.ok;
}

} // namespace ds
