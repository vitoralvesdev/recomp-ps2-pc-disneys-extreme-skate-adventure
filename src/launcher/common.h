#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ds {

namespace fs = std::filesystem;

std::string toUtf8(std::wstring_view text);
std::wstring toWide(std::string_view text);
std::wstring toWideLenient(std::string_view text);

std::wstring trim(std::wstring_view text);
std::string trimUtf8(std::string_view text);
bool iequals(std::wstring_view left, std::wstring_view right);
bool icontains(std::wstring_view text, std::wstring_view needle);

std::wstring formatBytes(uint64_t bytes);
std::wstring formatPercent(int percent);
std::wstring nowStamp();
std::wstring nowIso8601();

fs::path thisExecutablePath();
fs::path localAppDataDirectory();
std::wstring windowsErrorMessage(DWORD error);

bool readFileBytes(const fs::path& path, std::vector<uint8_t>& out, size_t maxBytes,
                   std::wstring& error);
bool readFilePrefix(const fs::path& path, void* buffer, size_t bytes, size_t& readBytes,
                    std::wstring& error);
bool writeFileAtomic(const fs::path& path, const void* data, size_t size, std::wstring& error);
bool writeTextFileAtomic(const fs::path& path, std::string_view utf8, std::wstring& error);
bool ensureDirectory(const fs::path& path, std::wstring& error);
bool fileExists(const fs::path& path);
uint64_t fileSize(const fs::path& path);

// SHA-256 of a file, lowercase hexadecimal. Used to identify the boot ELF and to
// document an installation without hashing the whole multi-gigabyte ISO.
bool sha256File(const fs::path& path, std::wstring& hexDigest, std::wstring& error);

// Quotes one Windows command line argument following the CommandLineToArgvW rules.
std::wstring quoteArgument(std::wstring_view argument);

class CancelToken {
public:
    void cancel() { cancelled_.store(true, std::memory_order_release); }
    bool cancelled() const { return cancelled_.load(std::memory_order_acquire); }

private:
    std::atomic<bool> cancelled_{false};
};

struct FileProgress {
    uint64_t completed = 0;
    uint64_t total = 0;
};

} // namespace ds
