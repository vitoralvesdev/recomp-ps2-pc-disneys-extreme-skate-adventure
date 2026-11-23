#include "package.h"

#include <algorithm>
#include <array>
#include <cstring>

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

} // namespace

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

    static constexpr const wchar_t* mediaDlls[] = {
        L"avcodec-61.dll", L"avformat-61.dll", L"avutil-59.dll", L"swresample-5.dll",
        L"swscale-8.dll"};
    std::vector<std::wstring> missingDlls;
    for (const wchar_t* dll : mediaDlls) {
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

    const fs::path catalog = layout.assetsDir / L"native" / L"audio" / L"catalog.tsv";
    if (!fileExists(catalog)) {
        add(L"Catálogo de áudio", CheckStatus::Warning,
            L"catalog.tsv ausente; o jogo pode abrir sem os efeitos sonoros.");
    } else {
        add(L"Catálogo de áudio", CheckStatus::Ok,
            L"catalog.tsv, " + formatBytes(fileSize(catalog)));
    }

    std::error_code ec;
    bool assetsPresent = fs::is_directory(layout.assetsDir / L"native", ec);
    if (!assetsPresent) {
        add(L"Assets nativos", CheckStatus::Warning,
            L"Pasta assets\\native ausente; fases e personagens podem não carregar.");
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
