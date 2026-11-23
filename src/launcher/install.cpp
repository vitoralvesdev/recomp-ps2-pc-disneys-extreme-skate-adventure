#include "install.h"

#include "log.h"
#include "package.h"
#include "child_process.h"
#include "version.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <sstream>

namespace ds {
namespace {

constexpr int kWeights[] = {5, 60, 15, 15, 5};
constexpr std::wstring_view kProgressPrefix = L"[DISNEY_PROGRESS] ";

int overallPercent(InstallStep step, int stepPercent) {
    int result = 0;
    for (int i = 0; i < static_cast<int>(step); ++i) result += kWeights[i];
    result += kWeights[static_cast<int>(step)] * std::clamp(stepPercent, 0, 100) / 100;
    return std::clamp(result, 0, 100);
}

std::string installStateText(const IsoValidation& validation, const fs::path& iso,
                             const std::wstring& launcherVersion) {
    std::ostringstream out;
    out << "[install]\r\n";
    out << "iso_path=" << toUtf8(iso.wstring()) << "\r\n";
    out << "serial=" << toUtf8(validation.serial) << "\r\n";
    out << "game_version=" << toUtf8(validation.version) << "\r\n";
    out << "boot_sha256=" << toUtf8(validation.bootSha256) << "\r\n";
    out << "installed_at=" << toUtf8(nowIso8601()) << "\r\n";
    out << "launcher_version=" << toUtf8(launcherVersion) << "\r\n";
    return out.str();
}

void emit(std::function<void(const InstallEvent&)> const& onEvent, InstallStep step,
          StepState state, int stepPercent, std::wstring message,
          std::wstring logLine = {}) {
    InstallEvent event;
    event.step = step;
    event.state = state;
    event.stepPercent = std::clamp(stepPercent, 0, 100);
    event.overallPercent = overallPercent(step, event.stepPercent);
    event.message = std::move(message);
    event.logLine = std::move(logLine);
    if (onEvent) onEvent(event);
}

bool runCommand(const std::wstring& command, const fs::path& sourceRoot,
                const CancelToken& cancel,
                const std::function<void(const InstallEvent&)>& onEvent, InstallStep step,
                int percentStart, int percentEnd, std::wstring& error) {
    logInfo(L"Executando: " + command);
    int percent = percentStart;
    DWORD exitCode = 0;
    const bool ran = runProcess(
        command, sourceRoot,
        [&](std::string_view line) {
            const std::wstring wide = trim(toWideLenient(line));
            if (wide.empty()) return;
            logInfo(L"[script] " + wide);
            int reported = 0;
            std::wstring progressMessage;
            const bool hasProgress = parseInstallProgress(wide, reported, progressMessage);
            if (hasProgress) {
                percent = std::max(percent, std::clamp(reported, percentStart, percentEnd));
            }
            InstallEvent event;
            event.step = step;
            event.state = StepState::Running;
            event.stepPercent = percent;
            event.overallPercent = overallPercent(step, percent);
            event.message = hasProgress ? progressMessage : wide;
            if (!hasProgress) event.logLine = wide;
            if (onEvent) onEvent(event);
        },
        cancel, exitCode, error);
    if (!ran) return false;
    if (exitCode != 0) {
        error = L"O comando terminou com código " + std::to_wstring(exitCode) +
                L". Consulte logs\\preparation e launcher.log.";
        return false;
    }
    return true;
}

bool runPowerShell(const fs::path& script, const std::vector<std::wstring>& arguments,
                   const fs::path& sourceRoot, const CancelToken& cancel,
                   const std::function<void(const InstallEvent&)>& onEvent, InstallStep step,
                   int percentStart, int percentEnd, std::wstring& error) {
    std::vector<std::wstring> commandArguments{L"-NoProfile", L"-ExecutionPolicy", L"Bypass",
                                               L"-File", script.wstring()};
    commandArguments.insert(commandArguments.end(), arguments.begin(), arguments.end());
    return runCommand(quoteExecutableCommand(L"powershell.exe", commandArguments), sourceRoot,
                      cancel, onEvent, step, percentStart, percentEnd, error);
}

bool runPython(const fs::path& script, const std::vector<std::wstring>& arguments,
               const fs::path& sourceRoot, const CancelToken& cancel,
               const std::function<void(const InstallEvent&)>& onEvent, InstallStep step,
               int percentStart, int percentEnd, std::wstring& error) {
    std::vector<std::wstring> commandArguments{script.wstring()};
    commandArguments.insert(commandArguments.end(), arguments.begin(), arguments.end());
    return runCommand(quoteExecutableCommand(L"python", commandArguments), sourceRoot, cancel,
                      onEvent, step, percentStart, percentEnd, error);
}

bool isInputReady(const IsoEntry& entry) { return !entry.directory; }

std::vector<const IsoEntry*> filesToExtract(IsoImage& image, const Layout& layout,
                                            bool force, uint64_t& totalBytes) {
    std::vector<const IsoEntry*> files;
    totalBytes = 0;
    for (const IsoEntry& entry : image.entries()) {
        if (!isInputReady(entry)) continue;
        const fs::path destination = layout.gameDir / toWide(entry.path);
        if (!force && fileExists(destination) && fileSize(destination) == entry.size) continue;
        files.push_back(&entry);
        totalBytes += entry.size;
    }
    return files;
}

bool requireSourceFiles(const fs::path& sourceRoot,
                        const std::vector<std::wstring>& relativePaths,
                        std::wstring& error) {
    std::wstring missing;
    for (const std::wstring& relative : relativePaths) {
        const fs::path path = sourceRoot / relative;
        if (!fileExists(path)) {
            if (!missing.empty()) missing += L"\n";
            missing += path.wstring();
        }
    }
    if (!missing.empty()) {
        error = L"Arquivos necessários ausentes no checkout fonte:\n" + missing +
                L"\n\nDefina source_root em config.ini ou restaure os arquivos indicados.";
        return false;
    }
    return true;
}

} // namespace

const wchar_t* installStepName(InstallStep step) {
    switch (step) {
    case InstallStep::Validate: return L"Verificando a imagem";
    case InstallStep::Extract: return L"Extraindo os arquivos do disco";
    case InstallStep::Assets: return L"Preparando dados, assets e áudio";
    case InstallStep::Runtime: return L"Recompilando e organizando o runtime";
    case InstallStep::Verify: return L"Verificando o pacote";
    default: return L"";
    }
}

int installStepWeight(InstallStep step) {
    const int index = static_cast<int>(step);
    return index >= 0 && index < static_cast<int>(InstallStep::Count) ? kWeights[index] : 0;
}

bool parseInstallProgress(std::wstring_view line, int& percent, std::wstring& message) {
    const std::wstring_view text = trim(line);
    if (!text.starts_with(kProgressPrefix)) return false;
    std::wstring_view remaining = text.substr(kProgressPrefix.size());
    const size_t separator = remaining.find(L' ');
    if (separator == std::wstring_view::npos) return false;
    const std::wstring_view number = remaining.substr(0, separator);
    if (number.empty()) return false;
    int value = 0;
    for (wchar_t digit : number) {
        if (digit < L'0' || digit > L'9') return false;
        value = value * 10 + (digit - L'0');
        if (value > 100) return false;
    }
    percent = value;
    message = std::wstring(trim(remaining.substr(separator + 1)));
    return !message.empty();
}

fs::path preparationSourceRoot(const Layout& layout, const Settings& settings) {
    if (!settings.sourceRoot.empty()) return settings.sourceRoot;
    if (layout.portable) return layout.dataRoot;
    return layout.exeDir;
}

bool gameDataMatchesIso(const Layout& layout, IsoImage& image, std::wstring& detail) {
    size_t missing = 0;
    size_t mismatched = 0;
    for (const IsoEntry& entry : image.entries()) {
        if (entry.directory) continue;
        const fs::path destination = layout.gameDir / toWide(entry.path);
        if (!fileExists(destination)) {
            ++missing;
        } else if (fileSize(destination) != entry.size) {
            ++mismatched;
        }
    }
    if (missing == 0 && mismatched == 0) {
        detail = L"Todos os arquivos do disco já estão presentes.";
        return true;
    }
    detail = L"Faltam " + std::to_wstring(missing) + L" arquivo(s) e " +
             std::to_wstring(mismatched) + L" precisam ser atualizados.";
    return false;
}

InstallResult performInstall(const Layout& layout, Settings& settings,
                             const InstallOptions& options, const CancelToken& cancel,
                             const std::function<void(const InstallEvent&)>& onEvent) {
    InstallResult result;
    const auto started = std::chrono::steady_clock::now();
    logInfo(L"Instalação iniciada: " + options.isoPath.wstring());
    logInfo(L"Destino: " + layout.gameDir.wstring());

    if (!ensureDirectory(layout.gameDir.parent_path(), result.message)) {
        emit(onEvent, InstallStep::Validate, StepState::Failed, 0, result.message);
        return result;
    }

    IsoImage image;
    emit(onEvent, InstallStep::Validate, StepState::Running, 0, L"Abrindo a imagem...");
    if (!image.open(options.isoPath, result.message)) {
        logError(result.message);
        emit(onEvent, InstallStep::Validate, StepState::Failed, 0, result.message);
        return result;
    }
    emit(onEvent, InstallStep::Validate, StepState::Running, 40, L"Conferindo serial e região...");
    if (!validateIso(image, result.validation)) {
        result.message = result.validation.errors.empty()
                             ? L"A imagem não foi reconhecida."
                             : result.validation.errors.front();
        if (result.validation.errors.size() > 1) {
            for (size_t i = 1; i < result.validation.errors.size(); ++i) {
                result.message += L"\n- " + result.validation.errors[i];
            }
        }
        logError(result.message);
        emit(onEvent, InstallStep::Validate, StepState::Failed, 100, result.message);
        return result;
    }
    for (const std::wstring& warning : result.validation.warnings) logWarning(warning);
    emit(onEvent, InstallStep::Validate, StepState::Done, 100,
         L"ISO NTSC-U " + result.validation.serial + L" validada");
    logInfo(L"ISO validada: " + result.validation.summary());

    emit(onEvent, InstallStep::Extract, StepState::Running, 0, L"Comparando os dados atuais...");
    uint64_t totalBytes = 0;
    std::vector<const IsoEntry*> pendingFiles =
        filesToExtract(image, layout, options.forceExtract, totalBytes);
    if (pendingFiles.empty()) {
        emit(onEvent, InstallStep::Extract, StepState::Skipped, 100,
             L"Os dados do jogo já estão completos; nada a extrair.");
    } else {
        if (!ensureDirectory(layout.gameDir, result.message)) {
            emit(onEvent, InstallStep::Extract, StepState::Failed, 0, result.message);
            return result;
        }
        logInfo(L"Extraindo " + std::to_wstring(pendingFiles.size()) + L" arquivo(s), " +
                formatBytes(totalBytes));
        uint64_t completed = 0;
        int lastPercent = -1;
        int lastMessagePercent = -100;
        const size_t totalFiles = pendingFiles.size();
        for (size_t index = 0; index < pendingFiles.size(); ++index) {
            if (cancel.cancelled()) {
                result.cancelled = true;
                result.message = L"Extração cancelada. Os arquivos já extraídos foram mantidos.";
                logWarning(result.message);
                emit(onEvent, InstallStep::Extract, StepState::Failed, 0, result.message);
                return result;
            }
            const IsoEntry& entry = *pendingFiles[index];
            const fs::path destination = layout.gameDir / toWide(entry.path);
            const int basePercent =
                totalBytes ? static_cast<int>(completed * 100 / totalBytes) : 0;
            if (basePercent - lastMessagePercent >= 2 || index + 1 == totalFiles) {
                lastMessagePercent = basePercent;
                emit(onEvent, InstallStep::Extract, StepState::Running, basePercent,
                     L"Extraindo " + toWide(entry.path) + L" (" + std::to_wstring(index + 1) +
                         L"/" + std::to_wstring(totalFiles) + L")");
            }
            const uint64_t entryBase = completed;
            if (!image.extract(
                    entry, destination, cancel,
                    [&](uint64_t fileDone) {
                        completed = entryBase + fileDone;
                        const int percent = totalBytes
                                                ? static_cast<int>(completed * 100 / totalBytes)
                                                : 0;
                        if (percent != lastPercent) {
                            lastPercent = percent;
                            InstallEvent event;
                            event.step = InstallStep::Extract;
                            event.state = StepState::Running;
                            event.stepPercent = percent;
                            event.overallPercent = overallPercent(InstallStep::Extract, percent);
                            event.message = L"Extraindo " + toWide(entry.path);
                            if (onEvent) onEvent(event);
                        }
                    },
                    result.message)) {
                if (cancel.cancelled()) {
                    result.cancelled = true;
                    result.message = L"Extração cancelada. Os arquivos já extraídos foram mantidos.";
                }
                logError(result.message);
                emit(onEvent, InstallStep::Extract, StepState::Failed,
                     totalBytes ? static_cast<int>(completed * 100 / totalBytes) : 0,
                     result.message);
                return result;
            }
            completed = entryBase + entry.size;
        }
        emit(onEvent, InstallStep::Extract, StepState::Done, 100,
             L"Dados extraídos: " + formatBytes(totalBytes));
        logInfo(L"Extração concluída em " +
                std::to_wstring(std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::steady_clock::now() - started)
                                    .count()) +
                L" s");
    }

    const fs::path sourceRoot = preparationSourceRoot(layout, settings);
    const fs::path analysisDir = layout.logsDir / L"preparation";
    const fs::path buildDir = layout.dataRoot / L"build" / L"setup";
    if (!ensureDirectory(analysisDir, result.message) ||
        !ensureDirectory(buildDir, result.message) ||
        !ensureDirectory(layout.assetsDir, result.message)) {
        emit(onEvent, InstallStep::Assets, StepState::Failed, 0, result.message);
        return result;
    }

    emit(onEvent, InstallStep::Assets, StepState::Running, 0, L"Conferindo assets e áudio...");
    std::wstring audioDetail;
    if (layout.hasAssets() && fileExists(layout.assetsDir / L"native" / L"spawn.txt") &&
        audioCatalogComplete(layout, audioDetail)) {
        emit(onEvent, InstallStep::Assets, StepState::Skipped, 100,
             L"Assets e áudio já preparados.");
    } else {
        if (!requireSourceFiles(
                sourceRoot,
                {L"scripts\\Preparar-Dados-Nativos.ps1",
                 L"scripts\\Instalar-Audios-Originais.py",
                 L"scripts\\Recuperar-Trilha-ISO.ps1",
                 L"scripts\\music-recovery\\install_music.py"},
                result.message)) {
            logError(result.message);
            emit(onEvent, InstallStep::Assets, StepState::Failed, 0, result.message);
            return result;
        }
        std::wstring scriptError;
        emit(onEvent, InstallStep::Assets, StepState::Running, 2,
             L"Convertendo fases, personagens, scripts e colisões...");
        if (!runPowerShell(sourceRoot / L"scripts" / L"Preparar-Dados-Nativos.ps1",
                           {L"-GameDir", layout.gameDir.wstring(), L"-AssetsDir",
                            layout.assetsDir.wstring(), L"-AnalysisDir", analysisDir.wstring(),
                            L"-BuildDir", buildDir.wstring()},
                           sourceRoot, cancel, onEvent, InstallStep::Assets, 2, 55,
                           scriptError)) {
            if (cancel.cancelled()) {
                result.cancelled = true;
                result.message = L"Preparação dos assets cancelada.";
            } else {
                result.message = L"A preparação dos assets falhou: " + scriptError;
            }
            logError(result.message);
            emit(onEvent, InstallStep::Assets, StepState::Failed, 0, result.message);
            return result;
        }
        emit(onEvent, InstallStep::Assets, StepState::Running, 58,
             L"Convertendo efeitos, vozes e catálogo de áudio...");
        if (!runPython(sourceRoot / L"scripts" / L"Instalar-Audios-Originais.py",
                       {layout.dataRoot.wstring(), L"--game-dir", layout.gameDir.wstring()},
                       sourceRoot, cancel, onEvent, InstallStep::Assets, 58, 85,
                       scriptError)) {
            if (cancel.cancelled()) {
                result.cancelled = true;
                result.message = L"Preparação do áudio cancelada.";
            } else {
                result.message =
                    L"A conversão do áudio falhou: " + scriptError +
                    L"\nOs pacotes WAV originais devem estar em %USERPROFILE%\\Downloads.";
            }
            logError(result.message);
            emit(onEvent, InstallStep::Assets, StepState::Failed, 0, result.message);
            return result;
        }
        emit(onEvent, InstallStep::Assets, StepState::Running, 87,
             L"Recuperando e verificando as trilhas da ISO...");
        if (!runPowerShell(sourceRoot / L"scripts" / L"Recuperar-Trilha-ISO.ps1",
                           {L"-Root", layout.dataRoot.wstring(), L"-GameDir",
                            layout.gameDir.wstring(), L"-BuildDir",
                            (buildDir / L"music-recovery").wstring()},
                           sourceRoot, cancel, onEvent, InstallStep::Assets, 87, 100,
                           scriptError)) {
            if (cancel.cancelled()) {
                result.cancelled = true;
                result.message = L"Recuperação das trilhas cancelada.";
            } else {
                result.message = L"A recuperação das trilhas falhou: " + scriptError;
            }
            logError(result.message);
            emit(onEvent, InstallStep::Assets, StepState::Failed, 0, result.message);
            return result;
        }
        if (!fileExists(layout.assetsDir / L"native" / L"spawn.txt") ||
            !audioCatalogComplete(layout, audioDetail)) {
            result.message = L"A preparação terminou sem os assets/áudio esperados: " + audioDetail;
            logError(result.message);
            emit(onEvent, InstallStep::Assets, StepState::Failed, 100, result.message);
            return result;
        }
        emit(onEvent, InstallStep::Assets, StepState::Done, 100,
             L"Assets, efeitos, vozes e trilhas preparados.");
    }

    emit(onEvent, InstallStep::Runtime, StepState::Running, 0, L"Conferindo o runtime...");
    if (runtimeFilesReady(layout)) {
        emit(onEvent, InstallStep::Runtime, StepState::Skipped, 100,
             L"Runtime e DLLs já instalados.");
    } else {
        if (!requireSourceFiles(
                sourceRoot,
                {L"scripts\\Recompilar.ps1",
                 L"scripts\\Compilar-Candidato-Original.ps1",
                 L"scripts\\Testar-Inicializacao-Original.ps1",
                 L"scripts\\prepare_guest_entries.py", L"scripts\\prepare_batches.py",
                 L"recomp\\extra-entry-points.json", L"recomp\\host-bridges.json",
                 L"analysis\\disjoint-initial-functions.json",
                 L"native\\original\\vu\\generated\\vu0\\vu1_programs.cpp",
                 L"native\\original\\iop\\generated\\iopn_registry.cpp",
                 L"CMakeLists.original-aot.txt"},
                result.message)) {
            result.message +=
                L"\n\nO clone limpo não inclui o gerador de VU/IOP. Use um checkout com "
                L"essas saídas derivadas já preparadas; o launcher não inventa microcódigo.";
            logError(result.message);
            emit(onEvent, InstallStep::Runtime, StepState::Failed, 0, result.message);
            return result;
        }
        std::wstring scriptError;
        emit(onEvent, InstallStep::Runtime, StepState::Running, 2,
             L"Recompilando as funções EE e preparando os lotes AOT...");
        if (!runPowerShell(sourceRoot / L"scripts" / L"Recompilar.ps1",
                           {L"-Config", L"ghidra", L"-ElfPath", layout.gameElf().wstring(),
                            L"-OutputDir", (sourceRoot / L"src" / L"generated-ghidra").wstring(),
                            L"-FunctionsCsv",
                            (sourceRoot / L"analysis" / L"functions.csv").wstring(),
                            L"-BuildDir", buildDir.wstring(), L"-AnalysisDir",
                            analysisDir.wstring(), L"-PrepareGuestEntries"},
                           sourceRoot, cancel, onEvent, InstallStep::Runtime, 2, 60,
                           scriptError)) {
            if (cancel.cancelled()) {
                result.cancelled = true;
                result.message = L"Recompilação cancelada.";
            } else {
                result.message = L"A recompilação das funções falhou: " + scriptError;
            }
            logError(result.message);
            emit(onEvent, InstallStep::Runtime, StepState::Failed, 0, result.message);
            return result;
        }
        emit(onEvent, InstallStep::Runtime, StepState::Running, 62,
             L"Compilando IOP, VU, runtime e organizando as DLLs...");
        if (!runPowerShell(sourceRoot / L"scripts" / L"Compilar-Candidato-Original.ps1",
                           {L"-SomenteRuntime", L"-GameDir", layout.gameDir.wstring(),
                            L"-BuildDir", buildDir.wstring(), L"-RuntimeOutputDir",
                            layout.runtimeDir.wstring(), L"-AnalysisDir", analysisDir.wstring()},
                           sourceRoot, cancel, onEvent, InstallStep::Runtime, 62, 100,
                           scriptError)) {
            if (cancel.cancelled()) {
                result.cancelled = true;
                result.message = L"Compilação cancelada.";
            } else {
                result.message = L"A compilação do runtime falhou: " + scriptError;
            }
            logError(result.message);
            emit(onEvent, InstallStep::Runtime, StepState::Failed, 0, result.message);
            return result;
        }
        if (!runtimeFilesReady(layout)) {
            result.message = L"O runtime foi compilado, mas o executável ou as DLLs de mídia "
                             L"não estão completos em " + layout.runtimeDir.wstring() + L".";
            logError(result.message);
            emit(onEvent, InstallStep::Runtime, StepState::Failed, 100, result.message);
            return result;
        }
        emit(onEvent, InstallStep::Runtime, StepState::Done, 100,
             L"Runtime e DLLs de mídia prontos.");
    }

    emit(onEvent, InstallStep::Verify, StepState::Running, 0, L"Verificando o pacote...");
    const PackageReport report = inspectPackage(layout);
    if (!report.ready) {
        result.message = L"O pacote ficou incompleto:";
        for (const CheckItem& item : report.items) {
            if (item.status == CheckStatus::Error) {
                result.message += L"\n- " + item.label + L": " + item.detail;
            }
        }
        logError(result.message);
        emit(onEvent, InstallStep::Verify, StepState::Failed, 0, result.message);
        return result;
    }
    const std::string stateText =
        installStateText(result.validation, options.isoPath, DISNEY_LAUNCHER_VERSION_STRING);
    std::wstring stateError;
    if (!writeTextFileAtomic(layout.installStatePath(), stateText, stateError)) {
        logWarning(L"Não foi possível gravar o estado da instalação: " + stateError);
    }
    settings.lastIsoPath = options.isoPath.wstring();

    result.ok = true;
    result.message = L"Instalação concluída. O jogo está pronto para abrir.";
    if (report.warningCount > 0) {
        result.message += L"\n" + std::to_wstring(report.warningCount) +
                          L" aviso(s); veja o diagnóstico abaixo.";
    }
    emit(onEvent, InstallStep::Verify, StepState::Done, 100, result.message);
    logInfo(L"Instalação concluída com sucesso.");
    return result;
}

} // namespace ds
