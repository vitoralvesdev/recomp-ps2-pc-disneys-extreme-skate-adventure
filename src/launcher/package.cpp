#include "package.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>

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

} // namespace

bool runtimeFilesReady(const Layout& layout) {
    if (!fileExists(layout.runtimeExe()) || fileSize(layout.runtimeExe()) < 8ull * 1024 * 1024) {
        return false;
    }
    for (const wchar_t* dll : kMediaDlls) {
        if (!fileExists(layout.runtimeDir / dll)) return false;
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
        if (!fileExists(layout.assetsDir.parent_path() / audioPath)) {
            detail = L"Arquivo de áudio ausente: " + toWide(relative);
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

PackageReport inspectPackage(const Layout& layout) {
    PackageReport report;
    auto add = [&](std::wstring label, CheckStatus status, std::wstring detail) {
        if (status == CheckStatus::Error) ++report.errorCount;
        if (status == CheckStatus::Warning) ++report.warningCount;
        report.items.push_back({std::move(label), status, std::move(detail)});
    };

    const fs::path runtime = layout.runtimeExe();
    if (!fileExists(runtime)) {
        add(L"Runtime do jogo", CheckStatus::Error,
            L"DisneySkateRuntime.exe não encontrado em " + layout.runtimeDir.wstring());
    } else if (fileSize(runtime) < 8ull * 1024 * 1024) {
        add(L"Runtime do jogo", CheckStatus::Error,
            L"O runtime parece incompleto: " + formatBytes(fileSize(runtime)));
    } else {
        report.runtimeStamp = fileDate(runtime);
        add(L"Runtime do jogo", CheckStatus::Ok,
            L"DisneySkateRuntime.exe, " + formatBytes(fileSize(runtime)) +
                (report.runtimeStamp.empty() ? L"" : L", " + report.runtimeStamp));
    }

    std::vector<std::wstring> missingDlls;
    for (const wchar_t* dll : kMediaDlls) {
        if (!fileExists(layout.runtimeDir / dll)) missingDlls.emplace_back(dll);
    }
    if (missingDlls.empty()) {
        add(L"Bibliotecas de mídia", CheckStatus::Ok, L"Decodificadores de áudio/vídeo presentes");
    } else {
        add(L"Bibliotecas de mídia", CheckStatus::Error,
            L"Faltam em runtime: " + joinMissing(missingDlls));
    }

    if (!fileExists(layout.gameElf())) {
        add(L"Dados originais", CheckStatus::Error,
            L"SLUS_206.07 não encontrado. A instalação a partir da ISO ainda não foi feita.");
    } else {
        std::array<uint8_t, 64> header{};
        size_t readBytes = 0;
        std::wstring error;
        bool elfOk = readFilePrefix(layout.gameElf(), header.data(), header.size(), readBytes,
                                    error) &&
                     readBytes == header.size() &&
                     std::memcmp(header.data(), "\x7f" "ELF", 4) == 0 && header[4] == 1 &&
                     header[18] == 8;
        if (!elfOk) {
            add(L"Dados originais", CheckStatus::Error,
                L"SLUS_206.07 existe, mas não é o ELF MIPS esperado.");
        } else {
            std::vector<std::wstring> missing;
            for (const wchar_t* name : {L"SKATE4.HED", L"SKATE4.WAD"}) {
                if (!fileExists(layout.gameDir / name)) missing.emplace_back(name);
            }
            if (missing.empty()) {
                add(L"Dados originais", CheckStatus::Ok,
                    L"Executável, SKATE4.HED e SKATE4.WAD presentes");
            } else {
                add(L"Dados originais", CheckStatus::Error,
                    L"Faltam: " + joinMissing(missing));
            }
        }
    }

    static constexpr const wchar_t* iopModules[] = {
        L"SIO2MAN.IRX", L"PADMAN.IRX", L"LIBSD.IRX", L"SDRDRV.IRX",
        L"MCMAN.IRX",   L"MCSERV.IRX", L"EZPCM.IRX"};
    std::vector<std::wstring> missingIop;
    for (const wchar_t* module : iopModules) {
        if (!fileExists(layout.gameDir / L"IOP" / module)) missingIop.emplace_back(module);
    }
    if (missingIop.empty()) {
        add(L"Módulos IOP", CheckStatus::Ok, L"7 módulos de entrada, áudio e memory card");
    } else {
        add(L"Módulos IOP", CheckStatus::Error,
            L"Faltam em " + (layout.gameDir / L"IOP").wstring() + L": " +
                joinMissing(missingIop));
    }

    std::wstring audioDetail;
    if (audioCatalogComplete(layout, audioDetail)) {
        add(L"Catálogo de áudio", CheckStatus::Ok, audioDetail);
    } else {
        add(L"Catálogo de áudio", CheckStatus::Error, audioDetail);
    }

    std::error_code ec;
    const bool assetsPresent = fs::is_directory(layout.assetsDir / L"native", ec);
    if (!assetsPresent || !fileExists(layout.assetsDir / L"native" / L"spawn.txt")) {
        add(L"Assets nativos", CheckStatus::Error,
            L"assets\\native\\spawn.txt ausente; execute a preparação dos dados nativos.");
    } else {
        add(L"Assets nativos", CheckStatus::Ok, L"Dados convertidos presentes");
    }

    if (!fileExists(layout.gameLog())) {
        add(L"Log da sessão", CheckStatus::Ok, L"Será criado ao iniciar o jogo");
    } else {
        add(L"Log da sessão", CheckStatus::Ok, layout.gameLog().wstring());
    }

    report.ready = report.errorCount == 0;
    return report;
}

} // namespace ds
