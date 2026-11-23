#pragma once

#include "common.h"

#include <deque>
#include <mutex>

namespace ds {

enum class LogLevel { Debug, Info, Warning, Error };

const wchar_t* logLevelName(LogLevel level);

class Logger {
public:
    static Logger& instance();

    bool open(const fs::path& path, std::wstring& error);
    void close();
    bool isOpen() const;

    void write(LogLevel level, std::wstring_view message);
    std::vector<std::wstring> tail(size_t maxLines) const;
    fs::path path() const;

private:
    Logger() = default;
    void rotateIfNeeded();

    mutable std::mutex mutex_;
    HANDLE file_ = INVALID_HANDLE_VALUE;
    fs::path path_;
    std::deque<std::wstring> recent_;
};

void logDebug(std::wstring_view message);
void logInfo(std::wstring_view message);
void logWarning(std::wstring_view message);
void logError(std::wstring_view message);

// Reads the tail of a UTF-8 or UTF-16 log file, returning at most maxLines
// lines. Used by the log viewer for the game session log and build logs.
std::wstring readLogTail(const fs::path& path, size_t maxBytes, size_t maxLines);

} // namespace ds
