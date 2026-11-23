#include "log.h"

#include <algorithm>
#include <cstdio>

namespace ds {
namespace {

std::wstring timestampForLine() {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%02u:%02u:%02u", time.wHour, time.wMinute, time.wSecond);
    return buffer;
}

} // namespace

const wchar_t* logLevelName(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return L"DEBUG";
    case LogLevel::Info: return L"INFO";
    case LogLevel::Warning: return L"AVISO";
    case LogLevel::Error: return L"ERRO";
    }
    return L"INFO";
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

bool Logger::open(const fs::path& path, std::wstring& error) {
    std::lock_guard lock(mutex_);
    if (file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
    path_ = path;
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    rotateIfNeeded();
    file_ = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ == INVALID_HANDLE_VALUE) {
        error = L"Não foi possível abrir o log " + path.wstring() + L": " +
                windowsErrorMessage(GetLastError());
        return false;
    }
    return true;
}

void Logger::close() {
    std::lock_guard lock(mutex_);
    if (file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
}

bool Logger::isOpen() const {
    std::lock_guard lock(mutex_);
    return file_ != INVALID_HANDLE_VALUE;
}

void Logger::rotateIfNeeded() {
    std::error_code ec;
    if (fs::file_size(path_, ec) < 4ull * 1024 * 1024) return;
    fs::path backup = path_;
    backup += L".old";
    fs::remove(backup, ec);
    fs::rename(path_, backup, ec);
}

void Logger::write(LogLevel level, std::wstring_view message) {
    std::lock_guard lock(mutex_);
    const std::wstring line =
        timestampForLine() + L" [" + logLevelName(level) + L"] " + std::wstring(message);
    recent_.push_back(line);
    while (recent_.size() > 4000) recent_.pop_front();
    if (file_ == INVALID_HANDLE_VALUE) return;
    const std::string utf8 = toUtf8(line) + "\r\n";
    DWORD written = 0;
    WriteFile(file_, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
}

std::vector<std::wstring> Logger::tail(size_t maxLines) const {
    std::lock_guard lock(mutex_);
    std::vector<std::wstring> result;
    const size_t count = std::min(maxLines, recent_.size());
    result.reserve(count);
    for (size_t i = recent_.size() - count; i < recent_.size(); ++i) result.push_back(recent_[i]);
    return result;
}

fs::path Logger::path() const {
    std::lock_guard lock(mutex_);
    return path_;
}

void logDebug(std::wstring_view message) { Logger::instance().write(LogLevel::Debug, message); }
void logInfo(std::wstring_view message) { Logger::instance().write(LogLevel::Info, message); }
void logWarning(std::wstring_view message) { Logger::instance().write(LogLevel::Warning, message); }
void logError(std::wstring_view message) { Logger::instance().write(LogLevel::Error, message); }

std::wstring readLogTail(const fs::path& path, size_t maxBytes, size_t maxLines) {
    std::error_code ec;
    const uint64_t size = fs::file_size(path, ec);
    if (ec || size == 0) return {};
    const uint64_t readSize = std::min<uint64_t>(size, maxBytes);
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER offset{};
    offset.QuadPart = static_cast<LONGLONG>(size - readSize);
    SetFilePointerEx(file, offset, nullptr, FILE_BEGIN);
    std::string bytes(static_cast<size_t>(readSize), '\0');
    DWORD totalRead = 0;
    while (totalRead < bytes.size()) {
        DWORD read = 0;
        if (!ReadFile(file, bytes.data() + totalRead,
                      static_cast<DWORD>(bytes.size() - totalRead), &read, nullptr) ||
            read == 0) {
            break;
        }
        totalRead += read;
    }
    bytes.resize(totalRead);
    CloseHandle(file);
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xff &&
        static_cast<unsigned char>(bytes[1]) == 0xfe) {
        return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data() + 2),
                            (bytes.size() - 2) / sizeof(wchar_t));
    }
    std::wstring text = toWideLenient(bytes);
    if (size > readSize) {
        const size_t newline = text.find(L'\n');
        if (newline != std::wstring::npos) text.erase(0, newline + 1);
    }
    size_t lines = 0;
    for (size_t i = text.size(); i > 0; --i) {
        if (text[i - 1] == L'\n' && ++lines > maxLines) {
            text.erase(0, i);
            break;
        }
    }
    return text;
}

} // namespace ds
