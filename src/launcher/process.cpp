#include "child_process.h"

#include <algorithm>
#include <cstdio>

namespace ds {
namespace {

struct JobCloser {
    HANDLE job = nullptr;
    ~JobCloser() {
        if (job) CloseHandle(job);
    }
};

HANDLE createKillOnCloseJob() {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) return nullptr;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits,
                                 sizeof(limits))) {
        CloseHandle(job);
        return nullptr;
    }
    return job;
}

void flushLines(std::string& pending, const std::function<void(std::string_view)>& onLine,
                bool force) {
    size_t newline = pending.find('\n');
    while (newline != std::string::npos) {
        std::string line = pending.substr(0, newline);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        pending.erase(0, newline + 1);
        if (onLine) onLine(line);
        newline = pending.find('\n');
    }
    if (force && !pending.empty()) {
        if (onLine) onLine(pending);
        pending.clear();
    }
}

} // namespace

std::wstring quoteExecutableCommand(const fs::path& executable,
                                    const std::vector<std::wstring>& arguments) {
    std::wstring command = quoteArgument(executable.wstring());
    for (const std::wstring& argument : arguments) {
        command += L' ';
        command += quoteArgument(argument);
    }
    return command;
}

bool runProcess(const std::wstring& commandLine, const fs::path& workingDirectory,
                const std::function<void(std::string_view)>& onLine, const CancelToken& cancel,
                DWORD& exitCode, std::wstring& error) {
    exitCode = 0;
    error.clear();

    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 64 * 1024)) {
        error = L"Não foi possível criar o pipe do processo: " +
                windowsErrorMessage(GetLastError());
        return false;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    HANDLE nullInput = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &security, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = nullInput;
    PROCESS_INFORMATION process{};
    std::wstring mutableCommand = commandLine;
    const std::wstring mutableDirectory = workingDirectory.wstring();

    const BOOL started = CreateProcessW(
        nullptr, mutableCommand.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr,
        mutableDirectory.empty() ? nullptr : mutableDirectory.c_str(), &startup, &process);
    if (!started) {
        error = L"Não foi possível iniciar o processo: " +
                windowsErrorMessage(GetLastError());
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        if (nullInput != INVALID_HANDLE_VALUE) CloseHandle(nullInput);
        return false;
    }
    CloseHandle(writePipe);
    writePipe = nullptr;
    if (nullInput != INVALID_HANDLE_VALUE) CloseHandle(nullInput);

    JobCloser job;
    job.job = createKillOnCloseJob();
    if (job.job) AssignProcessToJobObject(job.job, process.hProcess);

    std::string pending;
    char buffer[8192];
    bool cancelled = false;
    for (;;) {
        if (cancel.cancelled()) {
            cancelled = true;
            if (job.job) TerminateJobObject(job.job, 1);
            else TerminateProcess(process.hProcess, 1);
            break;
        }
        DWORD available = 0;
        if (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            DWORD read = 0;
            const DWORD chunk = std::min<DWORD>(available, sizeof(buffer));
            if (!ReadFile(readPipe, buffer, chunk, &read, nullptr) || read == 0) break;
            pending.append(buffer, read);
            flushLines(pending, onLine, false);
            continue;
        }
        const DWORD wait = WaitForSingleObject(process.hProcess, 20);
        if (wait == WAIT_OBJECT_0) {
            // Drain everything still buffered, then finish.
            for (;;) {
                DWORD buffered = 0;
                if (!PeekNamedPipe(readPipe, nullptr, 0, nullptr, &buffered, nullptr) ||
                    buffered == 0) {
                    break;
                }
                DWORD read = 0;
                const DWORD chunk = std::min<DWORD>(buffered, sizeof(buffer));
                if (!ReadFile(readPipe, buffer, chunk, &read, nullptr) || read == 0) break;
                pending.append(buffer, read);
                flushLines(pending, onLine, false);
            }
            break;
        }
    }
    flushLines(pending, onLine, true);

    if (cancelled) {
        WaitForSingleObject(process.hProcess, 5000);
    }
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(readPipe);
    if (cancelled) {
        error = L"Operação cancelada.";
        return false;
    }
    return true;
}

bool captureProcess(const std::wstring& commandLine, const fs::path& workingDirectory,
                    std::wstring& output, DWORD& exitCode, std::wstring& error,
                    const CancelToken* cancel, size_t maxBytes) {
    CancelToken localCancel;
    const CancelToken& token = cancel ? *cancel : localCancel;
    std::wstring collected;
    size_t bytes = 0;
    const bool ok = runProcess(
        commandLine, workingDirectory,
        [&](std::string_view line) {
            if (bytes >= maxBytes) return;
            const std::wstring wide = toWideLenient(line);
            collected += wide;
            collected += L'\n';
            bytes += wide.size() + 1;
        },
        token, exitCode, error);
    output = collected;
    return ok;
}

} // namespace ds
