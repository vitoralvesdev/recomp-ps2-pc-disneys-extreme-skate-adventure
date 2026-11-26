#pragma once

#include "common.h"

namespace ds {

// Runs a console executable with stdout/stderr merged into a pipe. onLine is
// called for each completed UTF-8/ANSI line; the callback runs on the calling
// thread. Cancelling terminates the whole child tree through a job object.
bool runProcess(const std::wstring& commandLine, const fs::path& workingDirectory,
                const std::function<void(std::string_view)>& onLine, const CancelToken& cancel,
                DWORD& exitCode, std::wstring& error,
                const fs::path& temporaryDirectory = {});

// Convenience wrapper that captures the merged output (bounded) instead of
// streaming it.
bool captureProcess(const std::wstring& commandLine, const fs::path& workingDirectory,
                    std::wstring& output, DWORD& exitCode, std::wstring& error,
                    const CancelToken* cancel = nullptr, size_t maxBytes = 256 * 1024);

std::wstring quoteExecutableCommand(const fs::path& executable,
                                    const std::vector<std::wstring>& arguments);

} // namespace ds
