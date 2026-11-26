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
#include "version.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <process.h>
#include <string>
#include <thread>
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

void writePeFile(const std::filesystem::path& path, uint8_t marker = 0) {
    std::vector<uint8_t> data(512, 0);
    data[100] = marker;
    data[0] = 'M';
    data[1] = 'Z';
    data[0x3c] = 64;
    data[64] = 'P';
    data[65] = 'E';
    data[68] = 0x64;
    data[69] = 0x86;
    data[88] = 0x0b;
    data[89] = 0x02;
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
    CHECK(ds::installerNameForTag(L"v0.4.0") == L"DisneySkateSetup-0.4.0.exe");
    CHECK(ds::installerNameForTag(L"0.4.0") == L"DisneySkateSetup-0.4.0.exe");
    CHECK(ds::installerNameForTag(L"v0.10.12") == L"DisneySkateSetup-0.10.12.exe");
    CHECK(ds::installerNameForTag(L"").empty());
    CHECK(ds::installerNameForTag(L"v").empty());
    CHECK(ds::installerNameForTag(L"v0.4.0/evil").empty());
    CHECK(ds::installerNameForTag(L"v0.4.0-beta").empty());
    CHECK(ds::installerNameForTag(L"v0..4").empty());

    TempDir temp;
    ds::CancelToken cancel;
    std::wstring error;
    const std::wstring assetName = L"DisneySkateSetup-0.4.0.exe";
    const std::wstring releasePath =
        L"/" DISNEY_LAUNCHER_REPOSITORY_OWNER L"/" DISNEY_LAUNCHER_REPOSITORY_NAME
        L"/releases/download/v0.4.0/";
    CHECK(!ds::downloadReleaseAsset(L"https://github.com" + releasePath + assetName,
                                    temp.path / assetName, L"v0.4.0", assetName, 0,
                                    cancel, {}, error));
    CHECK(!ds::downloadReleaseAsset(L"http://github.com" + releasePath + assetName,
                                    temp.path / assetName, L"v0.4.0", assetName, 123,
                                    cancel, {}, error));
    CHECK(!ds::downloadReleaseAsset(L"https://github.com" + releasePath + L"Other.exe",
                                    temp.path / assetName, L"v0.4.0", assetName, 123,
                                    cancel, {}, error));
    CHECK(!ds::downloadReleaseAsset(
        L"https://github.com" +
            std::wstring(L"/" DISNEY_LAUNCHER_REPOSITORY_OWNER L"/"
                         DISNEY_LAUNCHER_REPOSITORY_NAME L"/releases/download/v0.3.0/") +
            assetName,
        temp.path / assetName, L"v0.4.0", assetName, 123, cancel, {}, error));
}

void testConfig() {
    TempDir temp;
    const auto path = temp.path / L"config.ini";
    ds::Config config;
    std::wstring error;
    CHECK(config.load(path, error));
    CHECK_MSG(error.empty(), error);
    config.settings().lastIsoPath = L"C:\\Jogos\\Ação\\jogo.iso";
    config.settings().sourceRoot = L"C:\\Fontes\\DisneySkate";
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
    CHECK(loaded.settings().sourceRoot == L"C:\\Fontes\\DisneySkate");
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

    ds::Layout installed;
    installed.dataRoot = temp.path / L"user-data";
    installed.gameDir = installed.dataRoot / L"game";
    installed.runtimeDir = installed.dataRoot / L"runtime";
    installed.assetsDir = installed.dataRoot / L"assets";
    installed.savesDir = installed.dataRoot / L"saves" / L"profile-1";
    installed.logsDir = installed.dataRoot / L"logs";
    installed.stateDir = installed.dataRoot / L"launcher";
    installed.configPath = installed.stateDir / L"config.ini";
    std::wstring error;
    CHECK_MSG(installed.ensureUserDirectories(error), error);
    for (const auto& directory : {installed.dataRoot, installed.gameDir, installed.runtimeDir,
                                  installed.assetsDir, installed.savesDir, installed.logsDir,
                                  installed.stateDir}) {
        CHECK(std::filesystem::is_directory(directory));
    }
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

    writePeFile(layout.runtimeExe());
    for (const wchar_t* dll : {L"avcodec-61.dll", L"avformat-61.dll", L"avutil-59.dll",
                               L"swresample-5.dll", L"swscale-8.dll"}) {
        writePeFile(layout.runtimeDir / dll);
    }
    std::vector<char> elf(2 * 1024 * 1024, 0);
    elf[0] = 0x7f;
    elf[1] = 'E';
    elf[2] = 'L';
    elf[3] = 'F';
    elf[4] = 1;
    elf[5] = 1;
    elf[6] = 1;
    elf[16] = 2;
    elf[18] = 8;
    elf[24] = 1;
    writeBytes(layout.gameElf(), elf.data(), elf.size());
    writeFilledFile(layout.gameDir / L"SKATE4.HED", 4096);
    writeFilledFile(layout.gameDir / L"SKATE4.WAD", 11 * 1024 * 1024);
    for (const wchar_t* module : {L"SIO2MAN.IRX", L"PADMAN.IRX", L"LIBSD.IRX", L"SDRDRV.IRX",
                                  L"MCMAN.IRX", L"MCSERV.IRX", L"EZPCM.IRX"}) {
        writeFilledFile(layout.gameDir / L"IOP" / module, 2048);
    }
    std::string catalog = "sfx/test\tassets/native/audio/files/test.wav\t0\t0\n";
    for (int i = 0; i < 37; ++i) {
        catalog += "music/test" + std::to_string(i) +
                   "\tassets/native/audio/files/test.wav\t0\t0\n";
    }
    writeBytes(layout.assetsDir / L"native" / L"audio" / L"catalog.tsv", catalog.data(),
               catalog.size());
    writeFilledFile(layout.assetsDir / L"native" / L"audio" / L"files" / L"test.wav", 128);
    writeFilledFile(layout.assetsDir / L"native" / L"spawn.txt", 8);

    std::wstring error;
    std::wstring bootHash;
    CHECK_MSG(ds::sha256File(layout.gameElf(), bootHash, error), error);
    CHECK_MSG(ds::writePackageManifest(layout, bootHash, error), error);
    ds::PackageReport report = ds::inspectPackage(layout, bootHash);
    for (const ds::CheckItem& item : report.items) {
        if (item.status == ds::CheckStatus::Error) {
            std::wcerr << L"  pacote: " << item.label << L" -> " << item.detail << L"\n";
        }
    }
    CHECK_MSG(report.ready, L"erros: " + std::to_wstring(report.errorCount));
    CHECK(report.errorCount == 0);
    CHECK(report.find(L"Manifesto SHA-256") != nullptr);
    CHECK(ds::inspectPackageForLaunch(layout, bootHash).ready);

    std::error_code ec;
    CHECK(ds::runtimeFilesReady(layout));
    const std::string baseCatalog = catalog.substr(0, catalog.find('\n') + 1);
    writeBytes(layout.assetsDir / L"native" / L"audio" / L"catalog.tsv", baseCatalog.data(),
               baseCatalog.size());
    CHECK(!ds::audioCatalogComplete(layout, error));
    writeBytes(layout.assetsDir / L"native" / L"audio" / L"catalog.tsv", catalog.data(),
               catalog.size());
    CHECK_MSG(ds::audioCatalogComplete(layout, error), error);
    std::filesystem::remove(layout.assetsDir / L"native" / L"audio" / L"files" / L"test.wav", ec);
    report = ds::inspectPackage(layout, bootHash);
    CHECK(!report.ready);
    CHECK(report.errorCount >= 1);
    writeFilledFile(layout.assetsDir / L"native" / L"audio" / L"files" / L"test.wav", 128);
    std::filesystem::remove(layout.assetsDir / L"native" / L"audio" / L"catalog.tsv", ec);
    report = ds::inspectPackage(layout, bootHash);
    CHECK(!report.ready);
    CHECK(report.errorCount >= 1);
    writeBytes(layout.assetsDir / L"native" / L"audio" / L"catalog.tsv", catalog.data(),
               catalog.size());

    std::filesystem::remove(layout.runtimeDir / L"avcodec-61.dll", ec);
    report = ds::inspectPackage(layout, bootHash);
    CHECK(!report.ready);
    CHECK(report.errorCount >= 1);
    CHECK(!ds::runtimeFilesReady(layout));
    writePeFile(layout.runtimeDir / L"avcodec-61.dll");

    writePeFile(layout.runtimeDir / L"avutil-59.dll", 1);
    report = ds::inspectPackage(layout, bootHash);
    CHECK(!report.ready);
    CHECK(report.find(L"Manifesto SHA-256") != nullptr);
    CHECK(ds::inspectPackageForLaunch(layout, bootHash).ready);
    writePeFile(layout.runtimeDir / L"avutil-59.dll");

    const std::filesystem::path unlisted = layout.runtimeDir / L"unlisted.bin";
    writeFilledFile(unlisted, 4);
    report = ds::inspectPackage(layout, bootHash);
    CHECK(!report.ready);
    std::filesystem::remove(unlisted, ec);
    report = ds::inspectPackage(layout, bootHash);
    CHECK(report.ready);

    writeBytes(layout.gameDir / L"SKATE4.HED", "", 0);
    report = ds::inspectPackage(layout, bootHash);
    CHECK(!report.ready);
    writeFilledFile(layout.gameDir / L"SKATE4.HED", 4096);
    report = ds::inspectPackage(layout, bootHash);
    CHECK(report.ready);

    std::filesystem::remove(layout.stateDir / L"package-manifest.sha256", ec);
    report = ds::inspectPackage(layout, bootHash);
    CHECK(!report.ready);
    CHECK(ds::inspectPackageForLaunch(layout, bootHash).ready);
    CHECK_MSG(ds::writePackageManifest(layout, bootHash, error), error);
    CHECK(ds::inspectPackage(layout, bootHash).ready);

    writeBytes(layout.stateDir / L"package-manifest.sha256", "", 0);
    report = ds::inspectPackage(layout, bootHash);
    CHECK(!report.ready);
    CHECK_MSG(ds::writePackageManifest(layout, bootHash, error), error);
    CHECK(ds::inspectPackage(layout, bootHash).ready);
}

void testUninstall() {
    std::wstring error;
    auto installedLayout = [](const std::filesystem::path& root) {
        ds::Layout layout;
        layout.exePath = root / L"app" / L"launcher.exe";
        layout.exeDir = layout.exePath.parent_path();
        layout.dataRoot = root / L"user-data";
        layout.gameDir = layout.dataRoot / L"game";
        layout.runtimeDir = layout.dataRoot / L"runtime";
        layout.assetsDir = layout.dataRoot / L"assets";
        layout.savesDir = layout.dataRoot / L"saves" / L"profile-1";
        layout.logsDir = layout.dataRoot / L"logs";
        layout.stateDir = layout.dataRoot / L"launcher";
        layout.configPath = layout.stateDir / L"config.ini";
        return layout;
    };
    auto portableLayout = [](const std::filesystem::path& root, bool legacy) {
        ds::Layout layout;
        layout.exePath = root / L"launcher.exe";
        layout.exeDir = root;
        layout.dataRoot = root;
        layout.gameDir = legacy ? root / L"original" / L"extracted" : root / L"game";
        layout.runtimeDir = root / L"runtime";
        layout.assetsDir = root / L"assets";
        layout.savesDir = layout.gameDir / L"mc0";
        layout.logsDir = root / L"analysis";
        layout.stateDir = layout.logsDir / L"launcher";
        layout.configPath = layout.stateDir / L"config.ini";
        layout.portable = true;
        return layout;
    };
    auto populate = [](const ds::Layout& layout) {
        writeFilledFile(layout.exePath, 16);
        writeFilledFile(layout.runtimeExe(), 512);
        writeFilledFile(layout.gameElf(), 128);
        writeFilledFile(layout.gameDir / L"SKATE4.HED", 64);
        writeFilledFile(layout.assetsDir / L"native" / L"spawn.txt", 32);
        writeFilledFile(layout.savesDir / L"save.bin", 24);
        writeFilledFile(layout.launcherLog(), 48);
        writeFilledFile(layout.configPath, 40);
        writeFilledFile(layout.stateDir / L"package-manifest.sha256", 56);
        writeFilledFile(layout.installStatePath(), 72);
        writeFilledFile(layout.dataRoot / L"build" / L"setup" / "intermediate.bin", 80);
        writeFilledFile(layout.dataRoot / L"analysis" / L"relatorio.json", 96);
        writeFilledFile(layout.dataRoot / L"tools" / L"ferramenta.exe", 112);
    };

    {
        TempDir temp;
        const ds::Layout layout = installedLayout(temp.path);
        populate(layout);
        CHECK_MSG(ds::uninstallGame(layout, error), error);
        CHECK(!std::filesystem::exists(layout.gameDir));
        CHECK(!std::filesystem::exists(layout.assetsDir));
        CHECK(!std::filesystem::exists(layout.dataRoot / L"build" / L"setup"));
        CHECK(std::filesystem::exists(layout.runtimeExe()));
        CHECK(std::filesystem::exists(layout.savesDir / L"save.bin"));
        CHECK(std::filesystem::exists(layout.configPath));
        CHECK(std::filesystem::exists(layout.launcherLog()));
        CHECK(std::filesystem::exists(layout.dataRoot / L"analysis" / L"relatorio.json"));
        CHECK(std::filesystem::exists(layout.dataRoot / L"tools" / L"ferramenta.exe"));
        CHECK(!std::filesystem::exists(layout.stateDir / L"package-manifest.sha256"));
        CHECK(!std::filesystem::exists(layout.installStatePath()));
        CHECK(!ds::inspectPackageForLaunch(layout).ready);
        CHECK(!ds::writePackageManifest(layout, {}, error));
        CHECK_MSG(!error.empty(), L"writePackageManifest deveria recusar o pacote removido");
        CHECK_MSG(ds::uninstallGame(layout, error), error);
    }

    for (bool legacy : {true, false}) {
        TempDir temp;
        const ds::Layout layout = portableLayout(temp.path, legacy);
        populate(layout);
        CHECK_MSG(ds::uninstallGame(layout, error), error);
        CHECK(std::filesystem::is_directory(layout.gameDir));
        CHECK(!std::filesystem::exists(layout.gameElf()));
        CHECK(!std::filesystem::exists(layout.gameDir / L"SKATE4.HED"));
        CHECK(std::filesystem::exists(layout.savesDir / L"save.bin"));
        CHECK(std::filesystem::is_directory(layout.assetsDir / L"native"));
        CHECK(std::filesystem::is_empty(layout.assetsDir / L"native"));
        CHECK(std::filesystem::exists(layout.runtimeExe()));
        CHECK(std::filesystem::exists(layout.configPath));
        CHECK(std::filesystem::exists(layout.launcherLog()));
        CHECK(!std::filesystem::exists(layout.dataRoot / L"build" / L"setup"));
        const ds::Layout detected = ds::Layout::detect(layout.exePath);
        CHECK(detected.portable);
        CHECK(detected.gameDir == layout.gameDir);
        CHECK(detected.savesDir == layout.savesDir);
        CHECK_MSG(ds::uninstallGame(layout, error), error);
    }

    {
        TempDir temp;
        ds::Layout unsafe = installedLayout(temp.path);
        unsafe.gameDir = unsafe.dataRoot;
        populate(unsafe);
        CHECK(!ds::uninstallGame(unsafe, error));
        CHECK_MSG(!error.empty(), L"layout inseguro deveria retornar erro");
        CHECK(std::filesystem::exists(unsafe.savesDir / L"save.bin"));
        CHECK(std::filesystem::exists(unsafe.assetsDir / L"native" / L"spawn.txt"));
    }

    {
        TempDir temp;
        ds::Layout unsafe = installedLayout(temp.path);
        unsafe.assetsDir = unsafe.logsDir;
        populate(unsafe);
        CHECK(!ds::uninstallGame(unsafe, error));
        CHECK(std::filesystem::exists(unsafe.gameElf()));
        CHECK(std::filesystem::exists(unsafe.launcherLog()));
    }

    {
        TempDir temp;
        const ds::Layout layout = installedLayout(temp.path);
        populate(layout);
        HANDLE locked = CreateFileW(layout.gameElf().c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(locked != INVALID_HANDLE_VALUE);
        CHECK(!ds::uninstallGame(layout, error));
        CHECK(std::filesystem::exists(layout.gameElf()));
        CHECK_MSG(!error.empty(), L"arquivo bloqueado deveria retornar erro");
        CloseHandle(locked);
        CHECK_MSG(ds::uninstallGame(layout, error), error);
    }

    {
        TempDir temp;
        const ds::Layout layout = installedLayout(temp.path);
        populate(layout);
        const auto transient = layout.dataRoot / L"build" / L"setup" / L"dotnet-home" /
                               L"advertising.txt";
        writeFilledFile(transient, 24);
        CHECK(SetFileAttributesW(transient.c_str(), FILE_ATTRIBUTE_READONLY));
        HANDLE locked = CreateFileW(transient.c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(locked != INVALID_HANDLE_VALUE);
        if (locked != INVALID_HANDLE_VALUE) {
            std::thread release([locked] {
                std::this_thread::sleep_for(std::chrono::milliseconds(750));
                CloseHandle(locked);
            });
            CHECK_MSG(ds::uninstallGame(layout, error), error);
            release.join();
        }
        CHECK(!std::filesystem::exists(layout.dataRoot / L"build" / L"setup"));
        CHECK(std::filesystem::exists(layout.savesDir / L"save.bin"));
    }

    {
        TempDir temp;
        const ds::Layout layout = installedLayout(temp.path);
        populate(layout);
        const auto external = temp.path / L"external";
        const auto sentinel = external / L"save-sentinel.bin";
        writeFilledFile(sentinel, 24);
        const auto link = layout.gameDir / L"linked";
        std::error_code ec;
        std::filesystem::create_directory_symlink(external, link, ec);
        if (ec) {
            std::wcout << L"  (teste de link ignorado: privilégio indisponível: "
                       << ds::toWide(ec.message()) << L")\n";
        } else {
            CHECK_MSG(ds::uninstallGame(layout, error), error);
            CHECK(std::filesystem::exists(sentinel));
            CHECK(std::filesystem::symlink_status(link, ec).type() ==
                  std::filesystem::file_type::not_found);
        }
    }

    {
        TempDir temp;
        ds::Layout layout = installedLayout(temp.path);
        const auto external = temp.path / L"external-game";
        writeFilledFile(external / L"sentinel.bin", 24);
        std::error_code ec;
        std::filesystem::create_directories(layout.dataRoot, ec);
        std::filesystem::create_directory_symlink(external, layout.gameDir, ec);
        if (ec) {
            std::wcout << L"  (teste de raiz-link ignorada: privilégio indisponível: "
                       << ds::toWide(ec.message()) << L")\n";
        } else {
            CHECK(!ds::uninstallGame(layout, error));
            CHECK(std::filesystem::exists(external / L"sentinel.bin"));
            CHECK(std::filesystem::is_symlink(layout.gameDir));
        }
    }
}

void testInstallProgress() {
    int percent = -1;
    std::wstring message;
    CHECK(ds::parseInstallProgress(L"[DISNEY_PROGRESS] 37 Compilando o runtime", percent,
                                   message));
    CHECK(percent == 37);
    CHECK(message == L"Compilando o runtime");
    CHECK(ds::parseInstallProgress(L"  [DISNEY_PROGRESS] 100 Concluido  ", percent, message));
    CHECK(percent == 100);
    CHECK(!ds::parseInstallProgress(L"[DISNEY_PROGRESS] 101 Invalido", percent, message));
    CHECK(!ds::parseInstallProgress(L"[DISNEY_PROGRESS] x Invalido", percent, message));
    CHECK(!ds::parseInstallProgress(L"[DISNEY_PROGRESS] 10", percent, message));
    CHECK(!ds::parseInstallProgress(L"saida normal", percent, message));
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
    run(L"Desinstalação", testUninstall);
    run(L"Progresso", testInstallProgress);
    run(L"Processos", testProcess);
    run(L"ISO", testIso);
    std::wcout << L"\n" << (g_failures == 0 ? L"OK" : L"FALHAS") << L": " << g_checks
               << L" verificações, " << g_failures << L" falha(s)\n";
    return g_failures == 0 ? 0 : 1;
}
