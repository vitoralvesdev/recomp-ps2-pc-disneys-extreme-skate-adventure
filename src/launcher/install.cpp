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

constexpr int kWeights[] = {5, 70, 10, 10, 5};

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

bool copyTree(const fs::path& source, const fs::path& destination, const CancelToken& cancel,
              std::wstring& error, const std::function<void(uint64_t, uint64_t)>& onProgress = {}) {
    std::error_code ec;
    if (!fs::is_directory(source, ec)) return true;
    uint64_t total = 0;
    for (const auto& item : fs::recursive_directory_iterator(
             source, fs::directory_options::skip_permission_denied, ec)) {
        if (item.is_regular_file(ec)) total += item.file_size(ec);
    }
    uint64_t completed = 0;
    fs::create_directories(destination, ec);
    for (const auto& item : fs::recursive_directory_iterator(
             source, fs::directory_options::skip_permission_denied, ec)) {
        if (cancel.cancelled()) {
            error = L"Cópia cancelada.";
            return false;
        }
        if (!item.is_regular_file(ec)) continue;
        const fs::path relative = fs::relative(item.path(), source, ec);
        if (ec) continue;
        const fs::path target = destination / relative;
        if (fileExists(target) && fileSize(target) == item.file_size(ec)) {
            completed += item.file_size(ec);
            continue;
        }
        fs::create_directories(target.parent_path(), ec);
        fs::copy_file(item.path(), target, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            error = L"Falha ao copiar " + item.path().wstring() + L": " + toWide(ec.message());
            return false;
        }
        completed += item.file_size(ec);
        if (onProgress) onProgress(completed, total);
    }
    return true;
}

bool runPowerShell(const fs::path& script, const std::vector<std::wstring>& arguments,
                   const fs::path& sourceRoot, const CancelToken& cancel,
                   const std::function<void(const InstallEvent&)>& onEvent, InstallStep step,
                   int percentStart, int percentEnd, std::wstring& error) {
    std::wstring command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File " +
                           quoteArgument(script.wstring());
    for (const std::wstring& argument : arguments) command += L" " + quoteArgument(argument);
    logInfo(L"Executando: " + command);
    int percent = percentStart;
    DWORD exitCode = 0;
    const bool ran = runProcess(
        command, sourceRoot,
        [&](std::string_view line) {
            const std::wstring wide = trim(toWideLenient(line));
            if (wide.empty()) return;
            logInfo(L"[script] " + wide);
            InstallEvent event;
            event.step = step;
            event.state = StepState::Running;
            if (percent < percentEnd) percent += 1;
            event.stepPercent = percent;
            event.overallPercent = overallPercent(step, percent);
            event.message = wide.size() > 160 ? wide.substr(0, 157) + L"..." : wide;
            event.logLine = wide;
            if (onEvent) onEvent(event);
        },
        cancel, exitCode, error);
    if (!ran) return false;
    if (exitCode != 0) {
        error = L"O script terminou com código " + std::to_wstring(exitCode) +
                L". Consulte os logs em analysis\\.";
        return false;
    }
    return true;
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

} // namespace

const wchar_t* installStepName(InstallStep step) {
    switch (step) {
    case InstallStep::Validate: return L"Verificando a imagem";
    case InstallStep::Extract: return L"Extraindo os arquivos do disco";
    case InstallStep::Assets: return L"Preparando os dados do jogo";
    case InstallStep::Runtime: return L"Preparando o runtime";
    case InstallStep::Verify: return L"Verificando o pacote";
    default: return L"";
    }
}

int installStepWeight(InstallStep step) {
    const int index = static_cast<int>(step);
    return index >= 0 && index < static_cast<int>(InstallStep::Count) ? kWeights[index] : 0;
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

    // Extraction -------------------------------------------------------------
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

    // Prepared assets --------------------------------------------------------
    emit(onEvent, InstallStep::Assets, StepState::Running, 0, L"Conferindo assets nativos...");
    if (layout.hasAssets()) {
        if (fileExists(layout.assetsDir / L"native" / L"audio" / L"catalog.tsv")) {
            emit(onEvent, InstallStep::Assets, StepState::Skipped, 100,
                 L"Assets nativos já preparados.");
        } else {
            emit(onEvent, InstallStep::Assets, StepState::Warning, 100,
                 L"Assets presentes, mas o catálogo de áudio não foi encontrado; "
                 L"o jogo pode abrir sem efeitos sonoros.");
        }
    } else {
        const fs::path sourceRoot = preparationSourceRoot(layout, settings);
        const fs::path script = sourceRoot / L"scripts" / L"Preparar-Dados-Nativos.ps1";
        if (!fileExists(script)) {
            result.message =
                L"Os assets nativos ainda não foram preparados e o script de preparação não foi "
                L"encontrado.\n\nEsperado: " + script.wstring() +
                L"\n\nUse um checkout completo do projeto ou defina source_root no config.ini.";
            logError(result.message);
            emit(onEvent, InstallStep::Assets, StepState::Failed, 0, result.message);
            return result;
        }
        emit(onEvent, InstallStep::Assets, StepState::Running, 5,
             L"Executando a preparação dos assets (pode levar alguns minutos)...");
        std::wstring scriptError;
        if (!runPowerShell(script, {}, sourceRoot, cancel, onEvent, InstallStep::Assets, 5, 90,
                           scriptError)) {
            if (cancel.cancelled()) {
                result.cancelled = true;
                result.message = L"Preparação cancelada.";
            } else {
                result.message = L"A preparação dos assets falhou: " + scriptError;
            }
            logError(result.message);
            emit(onEvent, InstallStep::Assets, StepState::Failed, 0, result.message);
            return result;
        }
        if (!layout.portable) {
            emit(onEvent, InstallStep::Assets, StepState::Running, 92,
                 L"Copiando assets preparados para os dados do usuário...");
            std::wstring copyError;
            if (!copyTree(sourceRoot / L"assets" / L"native",
                          layout.assetsDir / L"native", cancel, copyError)) {
                result.message = L"Falha ao copiar os assets: " + copyError;
                logError(result.message);
                emit(onEvent, InstallStep::Assets, StepState::Failed, 0, result.message);
                return result;
            }
        }
        if (!layout.hasAssets()) {
            result.message = L"A preparação terminou, mas o catálogo de áudio ainda não existe.";
            logWarning(result.message);
            emit(onEvent, InstallStep::Assets, StepState::Warning, 100, result.message);
        } else {
            emit(onEvent, InstallStep::Assets, StepState::Done, 100, L"Assets preparados.");
        }
    }

    // Runtime ----------------------------------------------------------------
    emit(onEvent, InstallStep::Runtime, StepState::Running, 0, L"Conferindo o runtime...");
    if (layout.hasRuntime() &&
        fileExists(layout.runtimeDir / L"avcodec-61.dll") &&
        fileExists(layout.runtimeDir / L"avformat-61.dll")) {
        emit(onEvent, InstallStep::Runtime, StepState::Skipped, 100,
             L"Runtime já instalado.");
    } else {
        const fs::path sourceRoot = preparationSourceRoot(layout, settings);
        const fs::path script = sourceRoot / L"scripts" / L"Compilar-Candidato-Original.ps1";
        const fs::path generated = sourceRoot / L"src" / L"generated-ghidra";
        if (!fileExists(script) || !std::filesystem::is_directory(generated)) {
            result.message =
                L"O runtime do jogo (DisneySkateRuntime.exe) não está instalado e não é possível "
                L"compilá-lo nesta pasta.\n\nEsperado em " +
                layout.runtimeDir.wstring() +
                L".\nPara compilar a partir do código-fonte, use um checkout completo do projeto "
                L"com o código AOT gerado e defina source_root no config.ini.";
            logError(result.message);
            emit(onEvent, InstallStep::Runtime, StepState::Failed, 0, result.message);
            return result;
        }
        emit(onEvent, InstallStep::Runtime, StepState::Running, 5,
             L"Compilando o runtime (pode levar vários minutos)...");
        std::wstring scriptError;
        if (!runPowerShell(script, {}, sourceRoot, cancel, onEvent, InstallStep::Runtime, 5, 95,
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
        if (sourceRoot != layout.exeDir) {
            std::wstring copyError;
            if (!copyTree(sourceRoot / L"runtime", layout.runtimeDir, cancel, copyError)) {
                result.message = L"Runtime compilado, mas a cópia falhou: " + copyError;
                logError(result.message);
                emit(onEvent, InstallStep::Runtime, StepState::Failed, 0, result.message);
                return result;
            }
        }
        if (!layout.hasRuntime()) {
            result.message = L"O runtime foi compilado, mas não apareceu em " +
                             layout.runtimeDir.wstring() + L".";
            logError(result.message);
            emit(onEvent, InstallStep::Runtime, StepState::Failed, 0, result.message);
            return result;
        }
        emit(onEvent, InstallStep::Runtime, StepState::Done, 100, L"Runtime compilado.");
    }

    // Final verification -----------------------------------------------------
    emit(onEvent, InstallStep::Verify, StepState::Running, 0, L"Verificando o pacote...");
    const PackageReport report = inspectPackage(layout);
    if (!report.ready) {
        result.message = L"O pacote ficou incompleto:";
        for (const CheckItem& item : report.items) {
            if (item.status == CheckStatus::Error) result.message += L"\n- " + item.label + L": " + item.detail;
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
                          L" aviso(s) de mídia; veja o diagnóstico abaixo.";
    }
    emit(onEvent, InstallStep::Verify, StepState::Done, 100, result.message);
    logInfo(L"Instalação concluída com sucesso.");
    return result;
}

} // namespace ds
