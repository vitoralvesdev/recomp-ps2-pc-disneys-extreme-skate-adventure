#include "package.h"

#include "validate.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>

namespace ds {
namespace {

std::wstring fileDate(const fs::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return {};
    FILETIME local{};
    FileTimeToLocalFileTime(&data.ftLastWriteTime, &local);
    SYSTEMTIME time{};
    FileTimeToSystemTime(&local, &time);
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%02u/%02u/%04u", time.wDay, time.wMonth, time.wYear);
    return buffer;
}

std::wstring joinMissing(const std::vector<std::wstring>& missing) {
    std::wstring result;
    for (size_t i = 0; i < missing.size(); ++i) {
        if (i) result += L", ";
        result += missing[i];
    }
    return result;
}

constexpr const wchar_t* kMediaDlls[] = {
    L"avcodec-61.dll", L"avformat-61.dll", L"avutil-59.dll", L"swresample-5.dll",
    L"swscale-8.dll"};
constexpr const wchar_t* kGameFiles[] = {L"SLUS_206.07", L"SKATE4.HED", L"SKATE4.WAD"};
constexpr const wchar_t* kIopModules[] = {
    L"SIO2MAN.IRX", L"PADMAN.IRX", L"LIBSD.IRX", L"SDRDRV.IRX",
    L"MCMAN.IRX",   L"MCSERV.IRX", L"EZPCM.IRX"};
constexpr const wchar_t* kManifestHeader = L"# DisneySkate package SHA-256 v1";
constexpr const wchar_t* kManifestFile = L"package-manifest.sha256";

struct ManifestEntry {
    std::string logicalPath;
    uint64_t size = 0;
    std::wstring hash;
};

uint16_t readLe16(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}

uint32_t readLe32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

bool isNonEmptyPeFile(const fs::path& path) {
    const uint64_t size = fileSize(path);
    if (size < 64) return false;
    std::ifstream input(path, std::ios::binary);
    std::array<uint8_t, 64> dos{};
    input.read(reinterpret_cast<char*>(dos.data()), dos.size());
    if (input.gcount() != dos.size() || std::memcmp(dos.data(), "MZ", 2) != 0) return false;
    const uint32_t peOffset = readLe32(dos.data() + 0x3c);
    if (peOffset < dos.size() || peOffset > size - 28) return false;
    input.seekg(static_cast<std::streamoff>(peOffset));
    std::array<uint8_t, 28> pe{};
    input.read(reinterpret_cast<char*>(pe.data()), pe.size());
    return input.gcount() == pe.size() && std::memcmp(pe.data(), "PE\0\0", 4) == 0 &&
           readLe16(pe.data() + 4) == 0x8664 && readLe16(pe.data() + 24) == 0x20b;
}

bool hasExpectedElfHeader(const fs::path& path) {
    std::array<uint8_t, 64> header{};
    size_t readBytes = 0;
    std::wstring error;
    return readFilePrefix(path, header.data(), header.size(), readBytes, error) &&
           readBytes == header.size() && std::memcmp(header.data(), "\x7f" "ELF", 4) == 0 &&
           header[4] == 1 && header[5] == 1 && header[6] == 1 &&
           readLe16(header.data() + 16) == 2 && readLe16(header.data() + 18) == 8 &&
           readLe32(header.data() + 24) != 0;
}

fs::path packageManifestPath(const Layout& layout) {
    return layout.stateDir / kManifestFile;
}

std::string logicalManifestPath(const char* root, const fs::path& relative) {
    std::string result(root);
    for (const fs::path& part : relative) {
        result += '/';
        result += toUtf8(part.wstring());
    }
    return result;
}

bool resolveManifestPath(const Layout& layout, std::string_view logical, fs::path& path) {
    const char* rootName = nullptr;
    const fs::path* root = nullptr;
    if (logical.starts_with("runtime/")) {
        rootName = "runtime";
        root = &layout.runtimeDir;
    } else if (logical.starts_with("game/")) {
        rootName = "game";
        root = &layout.gameDir;
    } else if (logical.starts_with("assets/")) {
        rootName = "assets";
        root = &layout.assetsDir;
    } else {
        return false;
    }
    const std::string_view relative = logical.substr(std::string_view(rootName).size() + 1);
    if (relative.empty() || relative.find_first_of("\\:\0") != std::string_view::npos) {
        return false;
    }
    fs::path parsed;
    try {
        parsed = fs::u8path(relative);
    } catch (const fs::filesystem_error&) {
        return false;
    }
    if (parsed.is_absolute() || parsed.has_root_name() || parsed.has_root_directory()) return false;
    for (const fs::path& part : parsed) {
        if (part.empty() || part == L"." || part == L"..") return false;
    }
    path = *root / parsed;
    return true;
}

bool samePath(const fs::path& left, const fs::path& right) {
    return iequals(left.lexically_normal().wstring(), right.lexically_normal().wstring());
}

bool visitPackageFiles(
    const Layout& layout,
    const std::function<bool(const char*, const fs::path&, const fs::path&)>& visit,
    std::wstring& error) {
    auto addRoot = [&](const char* rootName, const fs::path& root, const fs::path* excluded) {
        std::error_code ec;
        if (!fs::is_directory(root, ec)) {
            error = L"Pasta ausente para o manifesto: " + root.wstring();
            return false;
        }
        fs::recursive_directory_iterator iterator(root, fs::directory_options::none, ec);
        if (ec) {
            error = L"Não foi possível percorrer " + root.wstring() + L": " +
                    toWide(ec.message());
            return false;
        }
        while (iterator != fs::recursive_directory_iterator()) {
            const fs::path current = iterator->path();
            if (excluded && samePath(current, *excluded)) {
                iterator.disable_recursion_pending();
            } else {
                const fs::file_status status = iterator->symlink_status(ec);
                if (ec) {
                    error = L"Não foi possível inspecionar " + current.wstring() + L": " +
                            toWide(ec.message());
                    return false;
                }
                if (fs::is_directory(status)) {
                } else if (!fs::is_regular_file(status)) {
                    error = L"Arquivo não regular no pacote: " + current.wstring();
                    return false;
                } else {
                    const uintmax_t size = iterator->file_size(ec);
                    if (ec || size == 0) {
                        error = (ec ? L"Não foi possível medir " : L"Arquivo vazio no pacote: ") +
                                current.wstring();
                        return false;
                    }
                    if (!visit(rootName, current, current.lexically_relative(root))) return false;
                }
            }
            iterator.increment(ec);
            if (ec) {
                error = L"Não foi possível percorrer " + root.wstring() + L": " +
                        toWide(ec.message());
                return false;
            }
        }
        return true;
    };

    return addRoot("runtime", layout.runtimeDir, nullptr) &&
           addRoot("game", layout.gameDir, layout.savesDir.empty() ? nullptr : &layout.savesDir) &&
           addRoot("assets", layout.assetsDir, nullptr);
}

bool collectManifestEntries(const Layout& layout, std::vector<ManifestEntry>& entries,
                            std::wstring& error) {
    const bool visited = visitPackageFiles(
        layout,
        [&](const char* rootName, const fs::path& path, const fs::path& relative) {
            std::wstring hash;
            if (!sha256File(path, hash, error)) return false;
            entries.push_back({logicalManifestPath(rootName, relative), fileSize(path),
                               std::move(hash)});
            return true;
        },
        error);
    if (!visited) return false;
    std::sort(entries.begin(), entries.end(), [](const ManifestEntry& left,
                                                 const ManifestEntry& right) {
        return left.logicalPath < right.logicalPath;
    });
    return true;
}

bool collectManifestPaths(const Layout& layout, std::set<std::string>& paths,
                          std::wstring& error) {
    return visitPackageFiles(
        layout,
        [&](const char* rootName, const fs::path&, const fs::path& relative) {
            paths.insert(logicalManifestPath(rootName, relative));
            return true;
        },
        error);
}

bool isSha256Digest(std::string_view digest) {
    if (digest.size() != 64) return false;
    for (char ch : digest) {
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
    }
    return true;
}

bool verifyPackageManifest(const Layout& layout, std::wstring& detail, std::wstring& error) {
    std::vector<uint8_t> bytes;
    if (!readFileBytes(packageManifestPath(layout), bytes, 16 * 1024 * 1024, error)) {
        error = L"Manifesto SHA-256 ausente ou ilegível: " + error;
        return false;
    }
    if (bytes.empty()) {
        error = L"Manifesto SHA-256 está vazio.";
        return false;
    }
    std::istringstream input(std::string(bytes.begin(), bytes.end()));
    std::string line;
    if (!std::getline(input, line)) {
        error = L"Manifesto SHA-256 está vazio.";
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != toUtf8(kManifestHeader)) {
        error = L"Cabeçalho do manifesto SHA-256 inválido.";
        return false;
    }

    std::set<std::string> seen;
    size_t count = 0;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) {
            error = L"Manifesto SHA-256 contém uma linha vazia.";
            return false;
        }
        const size_t firstTab = line.find('\t');
        const size_t secondTab = firstTab == std::string::npos
                                     ? std::string::npos
                                     : line.find('\t', firstTab + 1);
        if (firstTab == std::string::npos || secondTab == std::string::npos ||
            secondTab + 1 >= line.size() || line.find('\t', secondTab + 1) != std::string::npos) {
            error = L"Manifesto SHA-256 contém uma linha inválida.";
            return false;
        }
        const std::string digest = line.substr(0, firstTab);
        const std::string sizeText = line.substr(firstTab + 1, secondTab - firstTab - 1);
        const std::string logical = line.substr(secondTab + 1);
        uint64_t expectedSize = 0;
        const auto parsed = std::from_chars(sizeText.data(), sizeText.data() + sizeText.size(),
                                            expectedSize);
        if (!isSha256Digest(digest) || sizeText.empty() || expectedSize == 0 ||
            parsed.ec != std::errc{} || parsed.ptr != sizeText.data() + sizeText.size()) {
            error = L"Manifesto SHA-256 contém um hash ou tamanho inválido.";
            return false;
        }
        fs::path path;
        if (!resolveManifestPath(layout, logical, path)) {
            error = L"Manifesto SHA-256 contém um caminho inseguro: " + toWideLenient(logical);
            return false;
        }
        if (!seen.insert(logical).second) {
            error = L"Manifesto SHA-256 contém um caminho duplicado: " + toWideLenient(logical);
            return false;
        }
        if (!fileExists(path)) {
            error = L"Arquivo ausente segundo o manifesto: " + path.wstring();
            return false;
        }
        const uint64_t actualSize = fileSize(path);
        if (actualSize == 0 || actualSize != expectedSize) {
            error = L"Tamanho divergente no manifesto: " + path.wstring();
            return false;
        }
        std::wstring actualHash;
        if (!sha256File(path, actualHash, error)) return false;
        if (!iequals(actualHash, toWideLenient(digest))) {
            error = L"SHA-256 divergente no manifesto: " + path.wstring();
            return false;
        }
        ++count;
    }
    if (count == 0) {
        error = L"Manifesto SHA-256 não contém arquivos.";
        return false;
    }

    std::vector<std::string> missing;
    auto require = [&](std::string logical) {
        if (!seen.contains(logical)) missing.push_back(std::move(logical));
    };
    require("runtime/DisneySkateRuntime.exe");
    for (const wchar_t* dll : kMediaDlls) require("runtime/" + toUtf8(dll));
    for (const wchar_t* name : kGameFiles) require("game/" + toUtf8(name));
    for (const wchar_t* module : kIopModules) require("game/IOP/" + toUtf8(module));
    require("assets/native/spawn.txt");
    require("assets/native/audio/catalog.tsv");
    if (!missing.empty()) {
        std::wstring names;
        for (const std::string& name : missing) {
            if (!names.empty()) names += L", ";
            names += toWideLenient(name);
        }
        error = L"Arquivos obrigatórios ausentes do manifesto: " + names;
        return false;
    }
    std::set<std::string> actualPaths;
    if (!collectManifestPaths(layout, actualPaths, error)) {
        error = L"Não foi possível conferir os arquivos do pacote: " + error;
        return false;
    }
    if (actualPaths.size() != seen.size()) {
        error = L"Manifesto SHA-256 não descreve todos os arquivos do pacote.";
        return false;
    }
    for (const std::string& path : actualPaths) {
        if (!seen.contains(path)) {
            error = L"Arquivo ausente do manifesto SHA-256: " + toWideLenient(path);
            return false;
        }
    }
    detail = std::to_wstring(count) + L" arquivos com tamanho e SHA-256 verificados";
    return true;
}

} // namespace

bool runtimeFilesReady(const Layout& layout) {
    if (!isNonEmptyPeFile(layout.runtimeExe())) return false;
    for (const wchar_t* dll : kMediaDlls) {
        if (!isNonEmptyPeFile(layout.runtimeDir / dll)) return false;
    }
    return true;
}

bool audioCatalogComplete(const Layout& layout, std::wstring& detail) {
    const fs::path catalog = layout.assetsDir / L"native" / L"audio" / L"catalog.tsv";
    std::ifstream input(catalog);
    if (!input) {
        detail = L"catalog.tsv ausente; a etapa de áudio ainda não foi concluída.";
        return false;
    }
    std::string line;
    size_t count = 0;
    while (std::getline(input, line)) {
        if (count == 0 && line.starts_with("\xef\xbb\xbf")) line.erase(0, 3);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const size_t firstTab = line.find('\t');
        const size_t secondTab = firstTab == std::string::npos
                                     ? std::string::npos
                                     : line.find('\t', firstTab + 1);
        if (firstTab == std::string::npos || secondTab == std::string::npos ||
            firstTab == 0) {
            detail = L"catalog.tsv contém uma linha inválida.";
            return false;
        }
        const std::string relative = line.substr(firstTab + 1, secondTab - firstTab - 1);
        if (!relative.starts_with("assets/native/audio/")) {
            detail = L"catalog.tsv contém um caminho de áudio fora da pasta de assets.";
            return false;
        }
        const fs::path audioPath = fs::u8path(relative);
        if (audioPath.is_absolute()) {
            detail = L"catalog.tsv contém um caminho absoluto de áudio.";
            return false;
        }
        for (const auto& part : audioPath) {
            if (part == L"..") {
                detail = L"catalog.tsv contém um caminho de áudio inseguro.";
                return false;
            }
        }
        const fs::path resolvedAudio = layout.assetsDir.parent_path() / audioPath;
        if (!fileExists(resolvedAudio)) {
            detail = L"Arquivo de áudio ausente: " + toWide(relative);
            return false;
        }
        if (fileSize(resolvedAudio) == 0) {
            detail = L"Arquivo de áudio vazio: " + toWide(relative);
            return false;
        }
        ++count;
    }
    if (count == 0) {
        detail = L"catalog.tsv está vazio.";
        return false;
    }
    detail = std::to_wstring(count) + L" entradas; todos os arquivos referenciados estão presentes";
    return true;
}

const CheckItem* PackageReport::find(std::wstring_view label) const {
    for (const CheckItem& item : items) {
        if (item.label == label) return &item;
    }
    return nullptr;
}

PackageReport inspectPackageImpl(const Layout& layout, bool requireManifest,
                                  std::wstring_view bootReference) {
    PackageReport report;
    auto add = [&](std::wstring label, CheckStatus status, std::wstring detail) {
        if (status == CheckStatus::Error) ++report.errorCount;
        if (status == CheckStatus::Warning) ++report.warningCount;
        report.items.push_back({std::move(label), status, std::move(detail)});
    };
    const std::wstring referenceBootHash =
        bootReference.empty() ? expectedBootSha256() : std::wstring(bootReference);

    const fs::path runtime = layout.runtimeExe();
    if (!fileExists(runtime)) {
        add(L"Runtime do jogo", CheckStatus::Error,
            L"DisneySkateRuntime.exe não encontrado em " + layout.runtimeDir.wstring());
    } else if (!isNonEmptyPeFile(runtime)) {
        add(L"Runtime do jogo", CheckStatus::Error,
            L"DisneySkateRuntime.exe está vazio, truncado ou não é um PE x64 válido.");
    } else {
        report.runtimeStamp = fileDate(runtime);
        add(L"Runtime do jogo", CheckStatus::Ok,
            L"DisneySkateRuntime.exe, " + formatBytes(fileSize(runtime)) +
                (report.runtimeStamp.empty() ? L"" : L", " + report.runtimeStamp));
    }

    std::vector<std::wstring> missingDlls;
    std::vector<std::wstring> invalidDlls;
    for (const wchar_t* dll : kMediaDlls) {
        const fs::path path = layout.runtimeDir / dll;
        if (!fileExists(path)) missingDlls.emplace_back(dll);
        else if (!isNonEmptyPeFile(path)) invalidDlls.emplace_back(dll);
    }
    if (!missingDlls.empty() || !invalidDlls.empty()) {
        std::wstring detail;
        if (!missingDlls.empty()) detail += L"Faltam: " + joinMissing(missingDlls);
        if (!invalidDlls.empty()) {
            if (!detail.empty()) detail += L"; ";
            detail += L"Vazias ou inválidas: " + joinMissing(invalidDlls);
        }
        add(L"Bibliotecas de mídia", CheckStatus::Error, detail);
    } else {
        add(L"Bibliotecas de mídia", CheckStatus::Ok, L"Decodificadores de áudio/vídeo válidos");
    }

    if (!fileExists(layout.gameElf())) {
        add(L"Dados originais", CheckStatus::Error,
            L"SLUS_206.07 não encontrado. A instalação a partir da ISO ainda não foi feita.");
    } else if (!hasExpectedElfHeader(layout.gameElf())) {
        add(L"Dados originais", CheckStatus::Error,
            L"SLUS_206.07 está vazio, truncado ou não é um ELF MIPS de 32 bits válido.");
    } else {
        std::wstring bootHash;
        std::wstring hashError;
        if (!sha256File(layout.gameElf(), bootHash, hashError)) {
            add(L"Dados originais", CheckStatus::Error,
                L"Não foi possível verificar o SHA-256 do ELF: " + hashError);
        } else if (!iequals(bootHash, referenceBootHash)) {
            add(L"Dados originais", CheckStatus::Error,
                L"O SHA-256 de SLUS_206.07 não corresponde ao executável esperado.");
        } else {
            std::vector<std::wstring> missing;
            std::vector<std::wstring> empty;
            for (const wchar_t* name : {L"SKATE4.HED", L"SKATE4.WAD"}) {
                const fs::path path = layout.gameDir / name;
                if (!fileExists(path)) missing.emplace_back(name);
                else if (fileSize(path) == 0) empty.emplace_back(name);
            }
            if (!missing.empty() || !empty.empty()) {
                std::wstring detail;
                if (!missing.empty()) detail += L"Faltam: " + joinMissing(missing);
                if (!empty.empty()) {
                    if (!detail.empty()) detail += L"; ";
                    detail += L"Vazios: " + joinMissing(empty);
                }
                add(L"Dados originais", CheckStatus::Error, detail);
            } else {
                add(L"Dados originais", CheckStatus::Ok,
                    L"Executável, SKATE4.HED e SKATE4.WAD verificados");
            }
        }
    }

    std::vector<std::wstring> missingIop;
    std::vector<std::wstring> emptyIop;
    for (const wchar_t* module : kIopModules) {
        const fs::path path = layout.gameDir / L"IOP" / module;
        if (!fileExists(path)) missingIop.emplace_back(module);
        else if (fileSize(path) == 0) emptyIop.emplace_back(module);
    }
    if (!missingIop.empty() || !emptyIop.empty()) {
        std::wstring detail;
        if (!missingIop.empty()) detail += L"Faltam: " + joinMissing(missingIop);
        if (!emptyIop.empty()) {
            if (!detail.empty()) detail += L"; ";
            detail += L"Vazios: " + joinMissing(emptyIop);
        }
        add(L"Módulos IOP", CheckStatus::Error, detail);
    } else {
        add(L"Módulos IOP", CheckStatus::Ok, L"7 módulos de entrada, áudio e memory card");
    }

    std::wstring audioDetail;
    if (audioCatalogComplete(layout, audioDetail)) {
        add(L"Catálogo de áudio", CheckStatus::Ok, audioDetail);
    } else {
        add(L"Catálogo de áudio", CheckStatus::Error, audioDetail);
    }

    std::error_code ec;
    const bool assetsPresent = fs::is_directory(layout.assetsDir / L"native", ec);
    const fs::path spawn = layout.assetsDir / L"native" / L"spawn.txt";
    if (!assetsPresent || !fileExists(spawn)) {
        add(L"Assets nativos", CheckStatus::Error,
            L"assets\\native\\spawn.txt ausente; execute a preparação dos dados nativos.");
    } else if (fileSize(spawn) == 0) {
        add(L"Assets nativos", CheckStatus::Error, L"assets\\native\\spawn.txt está vazio.");
    } else {
        add(L"Assets nativos", CheckStatus::Ok, L"Dados convertidos presentes");
    }

    if (!fileExists(layout.gameLog())) {
        add(L"Log da sessão", CheckStatus::Ok, L"Será criado ao iniciar o jogo");
    } else {
        add(L"Log da sessão", CheckStatus::Ok, layout.gameLog().wstring());
    }

    if (requireManifest) {
        std::wstring manifestDetail;
        std::wstring manifestError;
        if (verifyPackageManifest(layout, manifestDetail, manifestError)) {
            add(L"Manifesto SHA-256", CheckStatus::Ok, manifestDetail);
        } else {
            add(L"Manifesto SHA-256", CheckStatus::Error, manifestError);
        }
    }

    report.ready = report.errorCount == 0;
    return report;
}

PackageReport inspectPackage(const Layout& layout, std::wstring_view bootReference) {
    return inspectPackageImpl(layout, true, bootReference);
}

bool writePackageManifest(const Layout& layout, std::wstring_view bootReference,
                          std::wstring& error) {
    error.clear();
    const PackageReport report = inspectPackageImpl(layout, false, bootReference);
    if (!report.ready) {
        error = L"Não foi possível gerar o manifesto porque o pacote está incompleto:";
        for (const CheckItem& item : report.items) {
            if (item.status == CheckStatus::Error) error += L"\n- " + item.label + L": " + item.detail;
        }
        return false;
    }

    std::vector<ManifestEntry> entries;
    if (!collectManifestEntries(layout, entries, error)) return false;
    std::ostringstream output;
    output << toUtf8(kManifestHeader) << '\n';
    for (const ManifestEntry& entry : entries) {
        output << toUtf8(entry.hash) << '\t' << entry.size << '\t' << entry.logicalPath << '\n';
    }
    if (!writeTextFileAtomic(packageManifestPath(layout), output.str(), error)) return false;
    std::wstring detail;
    return verifyPackageManifest(layout, detail, error);
}

} // namespace ds
