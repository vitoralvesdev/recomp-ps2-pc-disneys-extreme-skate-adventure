// Unit and integration tests for the native launcher core.
// The ISO integration tests run only when DISNEY_TEST_ISO points to a legal
// local image; the repository never ships game data.
#include "config.h"
#include "install.h"
#include "iso.h"
#include "json.h"
#include "layout.h"
#include "package.h"
#include "child_process.h"
#include "update.h"
#include "validate.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <process.h>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(condition)                                                                          \
    do {                                                                                          \
        ++g_checks;                                                                               \
        if (!(condition)) {                                                                       \
            std::wcerr << L"  FALHOU: " << #condition << L" (" << __LINE__ << L")\n";            \
            ++g_failures;                                                                         \
        }                                                                                         \
    } while (false)

#define CHECK_MSG(condition, message)                                                             \
    do {                                                                                          \
        ++g_checks;                                                                               \
        if (!(condition)) {                                                                       \
            std::wcerr << L"  FALHOU: " << #condition << L" (" << __LINE__ << L"): " << (message) \
                       << L"\n";                                                                  \
            ++g_failures;                                                                         \
        }                                                                                         \
    } while (false)

class TempDir {
public:
    TempDir() {
        static int counter = 0;
        std::error_code ec;
        path = std::filesystem::current_path(ec) / L"launcher-tests-work" /
               (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++counter));
        std::filesystem::create_directories(path, ec);
        if (ec) {
            std::wcerr << L"  (TempDir: falha ao criar " << path.wstring() << L": "
                       << ds::toWide(ec.message()) << L")\n";
        }
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    std::filesystem::path path;
};

void writeBytes(const std::filesystem::path& path, const void* data, size_t size) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, data, static_cast<DWORD>(size), &written, nullptr);
    CloseHandle(file);
}

void writeFilledFile(const std::filesystem::path& path, size_t size, char seed = 'A') {
    std::vector<char> data(size, seed);
    writeBytes(path, data.data(), data.size());
}

void testJson() {
    const char* sample = R"json({
        "tag_name": "v0.4.0",
        "name": "Launcher 0.4",
        "body": "linha 1\nlinha 2",
        "draft": false,
        "assets": [
            {"name": "DisneySkateSetup.exe", "size": 123456, "browser_download_url": "https://example.invalid/x.exe"}
        ]
    })json";
    std::string error;
    const auto parsed = ds::Json::parse(sample, &error);
    CHECK(parsed.has_value());
    if (parsed) {
        CHECK(parsed->isObject());
        CHECK(parsed->stringOr("tag_name") == "v0.4.0");
        CHECK(parsed->boolOr("draft", true) == false);
        const ds::Json* assets = parsed->get("assets");
        CHECK(assets && assets->isArray() && assets->array.size() == 1);
        if (assets && !assets->array.empty()) {
            CHECK(assets->array[0].numberOr("size") == 123456);
            CHECK(assets->array[0].stringOr("browser_download_url") ==
                  "https://example.invalid/x.exe");
        }
    }
    CHECK(!ds::Json::parse("{", &error).has_value());
    CHECK(!ds::Json::parse("{} extra", &error).has_value());
    const auto unicode = ds::Json::parse(R"json({"name":"A\u00e7\u00e3o \u2603"})json", &error);
    CHECK(unicode.has_value());
    if (unicode) CHECK(unicode->stringOr("name") == "Ação \xE2\x98\x83");
}

void testVersions() {
    CHECK(ds::compareVersions(L"0.3.0", L"0.4.0") < 0);
    CHECK(ds::compareVersions(L"v0.10.0", L"v0.9.9") > 0);
    CHECK(ds::compareVersions(L"0.3.0", L"v0.3.0") == 0);
    CHECK(ds::compareVersions(L"1.0.0", L"1.0.0-beta") > 0);
    CHECK(ds::compareVersions(L"1.2", L"1.2.0") == 0);
}

void testConfig() {
    TempDir temp;
    const auto path = temp.path / L"config.ini";
    ds::Config config;
    std::wstring error;
    CHECK(config.load(path, error));
    CHECK_MSG(error.empty(), error);
    config.settings().lastIsoPath = L"C:\\Jogos\\Ação\\jogo.iso";
    config.settings().renderScale = 3;
    config.settings().gpuRenderer = false;
    config.settings().nativeVblank = false;
    config.settings().hangDiagnostics = true;
    CHECK(config.save(path, error));
    CHECK_MSG(error.empty(), error);

    ds::Config loaded;
    CHECK(loaded.load(path, error));
    CHECK_MSG(error.empty(), error);
    CHECK(loaded.settings().lastIsoPath == L"C:\\Jogos\\Ação\\jogo.iso");
    CHECK(loaded.settings().renderScale == 3);
    CHECK(loaded.settings().gpuRenderer == false);
    CHECK(loaded.settings().nativeVblank == false);
    CHECK(loaded.settings().hangDiagnostics == true);

    const auto missing = temp.path / L"missing.ini";
    ds::Config defaults;
    CHECK(defaults.load(missing, error));
    CHECK(defaults.settings().renderScale == 1);
}

void testLayout() {
    TempDir temp;
    const auto exe = temp.path / L"Disney's Extreme Skate Adventure.exe";
    writeFilledFile(exe, 16);
    writeFilledFile(temp.path / L"runtime" / L"DisneySkateRuntime.exe", 16);
    writeFilledFile(temp.path / L"assets" / L"native" / L"audio" / L"catalog.tsv", 16);
    const ds::Layout layout = ds::Layout::detect(exe);
    CHECK(layout.portable);
    CHECK(layout.gameDir == temp.path / L"game");
    CHECK(layout.runtimeDir == temp.path / L"runtime");
    CHECK(layout.assetsDir == temp.path / L"assets");

    std::error_code ec;
    std::filesystem::create_directories(temp.path / L"original" / L"extracted", ec);
    const ds::Layout legacy = ds::Layout::detect(exe);
    CHECK(legacy.portable);
    CHECK(legacy.gameDir == temp.path / L"original" / L"extracted");
}

void testPackage() {
    TempDir temp;
    ds::Layout layout;
    layout.exePath = temp.path / L"launcher.exe";
    layout.exeDir = temp.path;
    layout.dataRoot = temp.path;
    layout.gameDir = temp.path / L"original" / L"extracted";
    layout.runtimeDir = temp.path / L"runtime";
    layout.assetsDir = temp.path / L"assets";
    layout.savesDir = layout.gameDir / L"mc0";
    layout.logsDir = temp.path / L"analysis";
    layout.stateDir = layout.logsDir / L"launcher";
    layout.portable = true;

    writeFilledFile(layout.runtimeExe(), 9 * 1024 * 1024);
    for (const wchar_t* dll : {L"avcodec-61.dll", L"avformat-61.dll", L"avutil-59.dll",
                               L"swresample-5.dll", L"swscale-8.dll"}) {
        writeFilledFile(layout.runtimeDir / dll, 16);
    }
    std::vector<char> elf(2 * 1024 * 1024, 0);
    elf[0] = 0x7f;
    elf[1] = 'E';
    elf[2] = 'L';
    elf[3] = 'F';
    elf[4] = 1;
    elf[5] = 1;
    elf[18] = 8;
    writeBytes(layout.gameElf(), elf.data(), elf.size());
    writeFilledFile(layout.gameDir / L"SKATE4.HED", 4096);
    writeFilledFile(layout.gameDir / L"SKATE4.WAD", 11 * 1024 * 1024);
    for (const wchar_t* module : {L"SIO2MAN.IRX", L"PADMAN.IRX", L"LIBSD.IRX", L"SDRDRV.IRX",
                                  L"MCMAN.IRX", L"MCSERV.IRX", L"EZPCM.IRX"}) {
        writeFilledFile(layout.gameDir / L"IOP" / module, 2048);
    }
    writeFilledFile(layout.assetsDir / L"native" / L"audio" / L"catalog.tsv", 128);
    writeFilledFile(layout.assetsDir / L"native" / L"spawn.txt", 8);

    ds::PackageReport report = ds::inspectPackage(layout);
    for (const ds::CheckItem& item : report.items) {
        if (item.status == ds::CheckStatus::Error) {
            std::wcerr << L"  pacote: " << item.label << L" -> " << item.detail << L"\n";
        }
    }
    CHECK_MSG(report.ready, L"erros: " + std::to_wstring(report.errorCount));
    CHECK(report.errorCount == 0);

    std::error_code ec;
    std::filesystem::remove(layout.assetsDir / L"native" / L"audio" / L"catalog.tsv", ec);
    report = ds::inspectPackage(layout);
    CHECK(report.ready);
    CHECK(report.warningCount >= 1);

    std::filesystem::remove(layout.runtimeDir / L"avcodec-61.dll", ec);
    report = ds::inspectPackage(layout);
    CHECK(!report.ready);
    CHECK(report.errorCount >= 1);
}

void testProcess() {
    TempDir temp;
    const std::wstring command = L"cmd.exe /c echo launcher-test-output";
    std::wstring output;
    DWORD exitCode = 0;
    std::wstring error;
    CHECK(ds::captureProcess(command, temp.path, output, exitCode, error));
    CHECK_MSG(error.empty(), error);
    CHECK(exitCode == 0);
    CHECK(output.find(L"launcher-test-output") != std::wstring::npos);
}

void testIso() {
    const wchar_t* isoEnvironment = _wgetenv(L"DISNEY_TEST_ISO");
    if (!isoEnvironment || !*isoEnvironment) {
        std::wcout << L"  (integração com ISO ignorada: DISNEY_TEST_ISO não definido)\n";
        return;
    }
    ds::IsoImage image;
    std::wstring error;
    CHECK_MSG(image.open(isoEnvironment, error), error);
    if (!image.isOpen()) return;
    CHECK(image.entries().size() >= 35);
    CHECK(image.payloadBytes() > 2ull * 1000 * 1000 * 1000);

    const ds::IsoEntry* cnf = image.find(L"SYSTEM.CNF");
    CHECK(cnf != nullptr);
    if (cnf) {
        std::vector<uint8_t> data;
        CHECK(image.readAll(*cnf, data, 4096, error));
        const std::wstring text = ds::toWideLenient(std::string(data.begin(), data.end()));
        CHECK(text.find(L"SLUS_206.07") != std::wstring::npos);
        CHECK(text.find(L"NTSC") != std::wstring::npos);
    }
    const ds::IsoEntry* boot = image.find(L"SLUS_206.07");
    CHECK(boot != nullptr);
    ds::IsoValidation validation;
    CHECK_MSG(ds::validateIso(image, validation), validation.summary());

    TempDir temp;
    if (boot) {
        const auto destination = temp.path / L"SLUS_206.07";
        ds::CancelToken cancel;
        uint64_t bytes = 0;
        error.clear();
        CHECK_MSG(image.extract(*boot, destination, cancel,
                                [&](uint64_t value) { bytes = value; }, error),
                  error);
        CHECK(bytes == boot->size);
        CHECK(ds::fileExists(destination));
        CHECK(ds::fileSize(destination) == boot->size);
        std::wstring digest;
        CHECK(ds::sha256File(destination, digest, error));
        CHECK_MSG(digest == validation.bootSha256, digest + L" != " + validation.bootSha256);
    }

    const wchar_t* extractedEnvironment = _wgetenv(L"DISNEY_TEST_EXTRACTED");
    if (extractedEnvironment && *extractedEnvironment) {
        const std::filesystem::path extracted(extractedEnvironment);
        ds::Layout layout;
        layout.gameDir = extracted;
        std::wstring detail;
        CHECK(ds::gameDataMatchesIso(layout, image, detail));
        std::wstring existingHash;
        if (boot && ds::fileExists(extracted / L"SLUS_206.07")) {
            CHECK(ds::sha256File(extracted / L"SLUS_206.07", existingHash, error));
            CHECK(existingHash == validation.bootSha256);
        }
    }
}

void run(const wchar_t* name, void (*function)()) {
    std::wcout << L"[" << name << L"]\n";
    function();
}

} // namespace

int wmain() {
    SetConsoleOutputCP(CP_UTF8);
    std::wcout << L"Disney's Extreme Skate Adventure - testes do launcher\n";
    run(L"JSON", testJson);
    run(L"Versões", testVersions);
    run(L"Configuração", testConfig);
    run(L"Layout", testLayout);
    run(L"Pacote", testPackage);
    run(L"Processos", testProcess);
    run(L"ISO", testIso);
    std::wcout << L"\n" << (g_failures == 0 ? L"OK" : L"FALHAS") << L": " << g_checks
               << L" verificações, " << g_failures << L" falha(s)\n";
    return g_failures == 0 ? 0 : 1;
}
