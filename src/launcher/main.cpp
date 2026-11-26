#include "app.h"
#include "config.h"
#include "install.h"
#include "layout.h"
#include "log.h"
#include "package.h"
#include "ui.h"
#include "update.h"
#include "version.h"

#include <shellapi.h>
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

HANDLE g_output = INVALID_HANDLE_VALUE;

void ensureOutput() {
    if (g_output != INVALID_HANDLE_VALUE && g_output != nullptr) return;
    HANDLE current = GetStdHandle(STD_OUTPUT_HANDLE);
    if (current && current != INVALID_HANDLE_VALUE) {
        g_output = current;
        return;
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        g_output = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    if (!g_output || g_output == INVALID_HANDLE_VALUE) {
        if (AllocConsole()) {
            SetConsoleOutputCP(CP_UTF8);
            g_output = GetStdHandle(STD_OUTPUT_HANDLE);
        }
    }
}

void write(const std::wstring& text) {
    ensureOutput();
    if (!g_output || g_output == INVALID_HANDLE_VALUE) return;
    const std::string utf8 = ds::toUtf8(text);
    DWORD written = 0;
    WriteFile(g_output, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
}

void writeLine(const std::wstring& text = L"") { write(text + L"\r\n"); }

int runVerify(const ds::Layout& layout) {
    ds::PackageReport report = ds::inspectPackage(layout);
    writeLine(L"Disney's Extreme Skate Adventure - verificação do pacote");
    writeLine(L"Launcher " DISNEY_LAUNCHER_VERSION_STRING);
    writeLine(layout.describe());
    writeLine();
    for (const ds::CheckItem& item : report.items) {
        const wchar_t* status = item.status == ds::CheckStatus::Ok        ? L"OK"
                                : item.status == ds::CheckStatus::Warning ? L"AVISO"
                                                                          : L"ERRO";
        writeLine(std::wstring(L"[") + status + L"] " + item.label + L": " + item.detail);
    }
    writeLine();
    writeLine(report.ready ? L"RESULTADO: pacote pronto para jogar."
                           : L"RESULTADO: a verificação falhou; confira os erros acima.");
    return report.ready ? 0 : 2;
}

int runManifest(const ds::Layout& layout) {
    std::wstring error;
    if (!layout.ensureUserDirectories(error)) {
        writeLine(L"ERRO: " + error);
        return 1;
    }
    if (!ds::writePackageManifest(layout, {}, error)) {
        writeLine(L"ERRO ao atualizar o manifesto: " + error);
        return 1;
    }
    writeLine(L"RESULTADO: manifesto SHA-256 atualizado para o pacote atual.");
    return 0;
}

int runIsoVerification(const ds::Layout& layout, const std::wstring& isoPath) {
    if (!ds::fileExists(isoPath)) {
        writeLine(L"ERRO: a imagem não existe: " + isoPath);
        return 2;
    }
    ds::IsoImage image;
    std::wstring error;
    if (!image.open(isoPath, error)) {
        writeLine(L"ERRO ao abrir a imagem: " + error);
        return 2;
    }
    ds::IsoValidation validation;
    if (!ds::validateIso(image, validation)) {
        writeLine(L"ISO inválida ou de outra região:");
        for (const std::wstring& item : validation.errors) writeLine(L"  - " + item);
        for (const std::wstring& item : validation.warnings) writeLine(L"  aviso: " + item);
        return 2;
    }
    writeLine(L"ISO válida: " + validation.serial +
              (validation.version.empty() ? L"" : L" (versão " + validation.version + L")"));
    writeLine(L"Arquivos: " + std::to_wstring(validation.fileCount) + L", " +
              ds::formatBytes(validation.payloadBytes));
    writeLine(L"SHA-256 do ELF: " + validation.bootSha256);
    ds::Layout checkLayout = layout;
    (void)checkLayout;
    return 0;
}

int runInstall(const ds::Layout& layout, const std::wstring& isoPath) {
    if (!ds::fileExists(isoPath)) {
        writeLine(L"ERRO: a imagem não existe: " + isoPath);
        return 2;
    }
    std::wstring error;
    if (!layout.ensureUserDirectories(error)) {
        writeLine(L"ERRO: " + error);
        return 1;
    }
    ds::Logger::instance().open(layout.launcherLog(), error);

    ds::Config config;
    config.load(layout.configPath, error);
    ds::Settings settings = config.settings();
    ds::CancelToken cancel;
    int lastPercent[5] = {-10, -10, -10, -10, -10};
    writeLine(L"Instalando a partir de " + isoPath);
    writeLine(L"Destino: " + layout.gameDir.wstring());
    writeLine();
    const ds::InstallResult result = ds::performInstall(
        layout, settings, {isoPath, false}, cancel,
        [&](const ds::InstallEvent& event) {
            const int index = static_cast<int>(event.step);
            if (index < 0 || index >= 5) return;
            if (!event.logLine.empty()) {
                writeLine(L"  " + event.logLine);
            }
            if (event.state == ds::StepState::Running) {
                if (event.stepPercent >= lastPercent[index] + 10) {
                    lastPercent[index] = event.stepPercent;
                    writeLine(L"  [" + std::to_wstring(event.stepPercent) + L"%] " +
                              event.message);
                }
            } else {
                lastPercent[index] = event.stepPercent;
                const wchar_t* state =
                    event.state == ds::StepState::Done       ? L"OK"
                    : event.state == ds::StepState::Skipped  ? L"OK (já pronto)"
                    : event.state == ds::StepState::Warning  ? L"AVISO"
                    : event.state == ds::StepState::Failed   ? L"FALHOU"
                                                             : L"...";
                writeLine(std::wstring(L"[") + state + L"] " + event.message);
            }
        });

    config.settings() = settings;
    config.save(layout.configPath, error);
    writeLine();
    writeLine(result.message);
    if (result.ok) {
        writeLine(L"Abra \"Disney's Extreme Skate Adventure.exe\" para jogar.");
        return 0;
    }
    return result.cancelled ? 3 : 1;
}

void printHelp() {
    writeLine(L"Disney's Extreme Skate Adventure - launcher nativo");
    writeLine();
    writeLine(L"Uso:");
    writeLine(L"  \"Disney's Extreme Skate Adventure.exe\"                 Abre o launcher.");
    writeLine(L"  \"...exe\" --verificar                                  Audita o pacote e os hashes SHA-256.");
    writeLine(L"  \"...exe\" --atualizar-manifesto                         Regera o manifesto apos recompilar o runtime.");
    writeLine(L"  \"...exe\" --iso <imagem> --verificar                   Valida uma ISO NTSC-U.");
    writeLine(L"  \"...exe\" --instalar <imagem>                          Instala sem interface.");
    writeLine(L"  \"...exe\" --diagnostico                                Abre com diagnóstico de travamentos.");
    writeLine(L"  \"...exe\" --versao                                     Mostra a versão.");
    writeLine();
    writeLine(L"A imagem deve ser a edição americana de PlayStation 2 (SLUS_206.07).");
    writeLine(L"O projeto não distribui a ISO, os dados do jogo nem os assets derivados.");
}

struct Arguments {
    bool verify = false;
    bool refreshManifest = false;
    bool diagnostics = false;
    bool help = false;
    bool version = false;
    std::wstring iso;
    std::wstring installIso;
    bool install = false;
};

Arguments parseArguments(int argc, wchar_t** argv) {
    Arguments result;
    for (int i = 1; i < argc; ++i) {
        const std::wstring argument = argv[i];
        if (_wcsicmp(argument.c_str(), L"--verificar") == 0) {
            result.verify = true;
        } else if (_wcsicmp(argument.c_str(), L"--atualizar-manifesto") == 0) {
            result.refreshManifest = true;
        } else if (_wcsicmp(argument.c_str(), L"--diagnostico") == 0) {
            result.diagnostics = true;
        } else if (_wcsicmp(argument.c_str(), L"--versao") == 0 ||
                   _wcsicmp(argument.c_str(), L"--version") == 0) {
            result.version = true;
        } else if (_wcsicmp(argument.c_str(), L"--ajuda") == 0 ||
                   _wcsicmp(argument.c_str(), L"--help") == 0) {
            result.help = true;
        } else if (_wcsicmp(argument.c_str(), L"--iso") == 0 && i + 1 < argc) {
            result.iso = argv[++i];
        } else if (_wcsicmp(argument.c_str(), L"--instalar") == 0) {
            result.install = true;
            if (i + 1 < argc && argv[i + 1][0] != L'-') result.installIso = argv[++i];
        }
    }
    if (result.installIso.empty()) result.installIso = result.iso;
    return result;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    const Arguments arguments = parseArguments(__argc, __wargv);
    if (arguments.help) {
        printHelp();
        return 0;
    }
    if (arguments.version) {
        writeLine(L"Disney's Extreme Skate Adventure launcher " DISNEY_LAUNCHER_VERSION_STRING);
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ds::Layout layout = ds::Layout::detect(ds::thisExecutablePath());
    if (arguments.refreshManifest) {
        ensureOutput();
        const int result = runManifest(layout);
        CoUninitialize();
        return result;
    }
    if (arguments.verify) {
        ensureOutput();
        const int result = arguments.iso.empty() ? runVerify(layout)
                                                 : runIsoVerification(layout, arguments.iso);
        CoUninitialize();
        return result;
    }
    if (arguments.install) {
        ensureOutput();
        int result = 1;
        if (arguments.installIso.empty()) {
            writeLine(L"ERRO: informe a imagem com --instalar <arquivo.iso>.");
            result = 2;
        } else {
            result = runInstall(layout, arguments.installIso);
        }
        CoUninitialize();
        return result;
    }

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\HighSky_DisneySkate_Launcher_v2");
    if (!mutex) {
        CoUninitialize();
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(ds::App::className(), nullptr)) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(mutex);
        CoUninitialize();
        return 0;
    }

    ds::App& app = ds::App::instance();
    if (!app.initialize(instance, arguments.diagnostics)) {
        MessageBoxW(nullptr, L"Não foi possível iniciar o launcher.", L"Disney's Extreme Skate Adventure",
                    MB_OK | MB_ICONERROR);
        CloseHandle(mutex);
        CoUninitialize();
        return 1;
    }
    const int result = app.run();
    app.shutdown();
    ds::ui::shutdown();
    CloseHandle(mutex);
    CoUninitialize();
    return result;
}
