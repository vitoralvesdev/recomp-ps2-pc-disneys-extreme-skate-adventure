#include "common.h"

#include <bcrypt.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <deque>
#include <memory>

#pragma comment(lib, "bcrypt.lib")

namespace ds {
namespace {

std::wstring hexDigest(const unsigned char* data, size_t size) {
    static const wchar_t digits[] = L"0123456789abcdef";
    std::wstring result;
    result.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) {
        result.push_back(digits[data[i] >> 4]);
        result.push_back(digits[data[i] & 0x0f]);
    }
    return result;
}

} // namespace

std::string toUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                        length, nullptr, nullptr);
    return result;
}

std::wstring toWide(std::string_view text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                        length);
    return result;
}

std::wstring toWideLenient(std::string_view text) {
    if (text.empty()) return {};
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                     static_cast<int>(text.size()), nullptr, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (length <= 0) {
        codePage = CP_ACP;
        flags = 0;
        length = MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()),
                                     nullptr, 0);
        if (length <= 0) return {};
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()), result.data(),
                        length);
    return result;
}

std::wstring trim(std::wstring_view text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::iswspace(text[begin])) ++begin;
    while (end > begin && std::iswspace(text[end - 1])) --end;
    return std::wstring(text.substr(begin, end - begin));
}

std::string trimUtf8(std::string_view text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && static_cast<unsigned char>(text[begin]) <= 0x20) ++begin;
    while (end > begin && static_cast<unsigned char>(text[end - 1]) <= 0x20) --end;
    return std::string(text.substr(begin, end - begin));
}

bool iequals(std::wstring_view left, std::wstring_view right) {
    if (left.size() != right.size()) return false;
    for (size_t i = 0; i < left.size(); ++i) {
        if (std::towlower(left[i]) != std::towlower(right[i])) return false;
    }
    return true;
}

bool icontains(std::wstring_view text, std::wstring_view needle) {
    if (needle.empty()) return true;
    if (needle.size() > text.size()) return false;
    for (size_t i = 0; i + needle.size() <= text.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (std::towlower(text[i + j]) != std::towlower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

std::wstring formatBytes(uint64_t bytes) {
    wchar_t buffer[64]{};
    if (bytes >= 1000ull * 1000ull * 1000ull) {
        swprintf_s(buffer, L"%.2f GB", static_cast<double>(bytes) / (1000.0 * 1000.0 * 1000.0));
    } else if (bytes >= 1000ull * 1000ull) {
        swprintf_s(buffer, L"%.1f MB", static_cast<double>(bytes) / (1000.0 * 1000.0));
    } else if (bytes >= 1000ull) {
        swprintf_s(buffer, L"%.1f KB", static_cast<double>(bytes) / 1000.0);
    } else {
        swprintf_s(buffer, L"%llu bytes", static_cast<unsigned long long>(bytes));
    }
    std::wstring result(buffer);
    for (wchar_t& ch : result) {
        if (ch == L'.') ch = L',';
    }
    return result;
}

std::wstring formatPercent(int percent) {
    return std::to_wstring(percent) + L"%";
}

std::wstring nowStamp() {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u:%02u", time.wYear, time.wMonth, time.wDay,
               time.wHour, time.wMinute, time.wSecond);
    return buffer;
}

std::wstring nowIso8601() {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%04u-%02u-%02uT%02u:%02u:%02u", time.wYear, time.wMonth, time.wDay,
               time.wHour, time.wMinute, time.wSecond);
    return buffer;
}

fs::path thisExecutablePath() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return {};
    buffer.resize(length);
    return fs::path(buffer);
}

fs::path localAppDataDirectory() {
    wchar_t buffer[32768]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 32768);
    if (length > 0 && length < 32768) return fs::path(buffer);
    PWSTR known = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &known)) && known) {
        fs::path result(known);
        CoTaskMemFree(known);
        return result;
    }
    return fs::temp_directory_path();
}

std::wstring windowsErrorMessage(DWORD error) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring result;
    if (length && buffer) {
        result.assign(buffer, length);
        while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' ||
                                   result.back() == L' ')) {
            result.pop_back();
        }
    }
    if (buffer) LocalFree(buffer);
    if (result.empty()) result = L"código " + std::to_wstring(error);
    return result;
}

bool readFileBytes(const fs::path& path, std::vector<uint8_t>& out, size_t maxBytes,
                   std::wstring& error) {
    out.clear();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Não foi possível abrir " + path.wstring() + L": " +
                windowsErrorMessage(GetLastError());
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size)) {
        error = L"Não foi possível medir " + path.wstring() + L": " +
                windowsErrorMessage(GetLastError());
        CloseHandle(file);
        return false;
    }
    if (size.QuadPart < 0 || static_cast<uint64_t>(size.QuadPart) > maxBytes) {
        error = L"Arquivo grande demais para leitura: " + path.wstring();
        CloseHandle(file);
        return false;
    }
    out.resize(static_cast<size_t>(size.QuadPart));
    size_t offset = 0;
    while (offset < out.size()) {
        DWORD read = 0;
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(out.size() - offset, 1u << 20));
        if (!ReadFile(file, out.data() + offset, chunk, &read, nullptr) || read == 0) {
            error = L"Falha ao ler " + path.wstring() + L": " + windowsErrorMessage(GetLastError());
            CloseHandle(file);
            return false;
        }
        offset += read;
    }
    CloseHandle(file);
    return true;
}

bool readFilePrefix(const fs::path& path, void* buffer, size_t bytes, size_t& readBytes,
                    std::wstring& error) {
    readBytes = 0;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Não foi possível abrir " + path.wstring() + L": " +
                windowsErrorMessage(GetLastError());
        return false;
    }
    DWORD read = 0;
    const DWORD chunk = static_cast<DWORD>(std::min<size_t>(bytes, 1u << 20));
    const bool ok = ReadFile(file, buffer, chunk, &read, nullptr) != FALSE;
    CloseHandle(file);
    if (!ok) {
        error = L"Falha ao ler " + path.wstring() + L": " + windowsErrorMessage(GetLastError());
        return false;
    }
    readBytes = read;
    return true;
}

bool writeFileAtomic(const fs::path& path, const void* data, size_t size, std::wstring& error) {
    std::error_code ec;
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path(), ec);
    fs::path temporary = path;
    temporary += L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Não foi possível gravar " + temporary.wstring() + L": " +
                windowsErrorMessage(GetLastError());
        return false;
    }
    size_t offset = 0;
    while (offset < size) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(size - offset, 1u << 20));
        if (!WriteFile(file, static_cast<const uint8_t*>(data) + offset, chunk, &written, nullptr) ||
            written == 0) {
            error = L"Falha ao gravar " + temporary.wstring() + L": " +
                    windowsErrorMessage(GetLastError());
            CloseHandle(file);
            DeleteFileW(temporary.c_str());
            return false;
        }
        offset += written;
    }
    FlushFileBuffers(file);
    CloseHandle(file);
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = L"Não foi possível substituir " + path.wstring() + L": " +
                windowsErrorMessage(GetLastError());
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

bool writeTextFileAtomic(const fs::path& path, std::string_view utf8, std::wstring& error) {
    return writeFileAtomic(path, utf8.data(), utf8.size(), error);
}

bool ensureDirectory(const fs::path& path, std::wstring& error) {
    std::error_code ec;
    fs::create_directories(path, ec);
    if (ec && !fs::is_directory(path)) {
        error = L"Não foi possível criar a pasta " + path.wstring() + L": " +
                toWide(ec.message());
        return false;
    }
    return true;
}

bool fileExists(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

uint64_t fileSize(const fs::path& path) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    return ec ? 0 : static_cast<uint64_t>(size);
}

bool sha256File(const fs::path& path, std::wstring& hexDigestOut, std::wstring& error) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<uint8_t> hashObject;
    std::vector<uint8_t> digest;
    HANDLE file = INVALID_HANDLE_VALUE;
    bool ok = false;

    do {
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
            error = L"Não foi possível iniciar o SHA-256.";
            break;
        }
        DWORD objectSize = 0;
        DWORD digestSize = 0;
        DWORD received = 0;
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                              reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &received,
                              0) < 0 ||
            BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&digestSize),
                              sizeof(digestSize), &received, 0) < 0) {
            error = L"Não foi possível consultar o SHA-256.";
            break;
        }
        hashObject.resize(objectSize);
        digest.resize(digestSize);
        if (BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize, nullptr, 0, 0) < 0) {
            error = L"Não foi possível criar o hash SHA-256.";
            break;
        }
        file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                           nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            error = L"Não foi possível abrir " + path.wstring() + L": " +
                    windowsErrorMessage(GetLastError());
            break;
        }
        std::vector<uint8_t> buffer(1 << 20);
        for (;;) {
            DWORD read = 0;
            if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
                error = L"Falha ao ler " + path.wstring() + L": " +
                        windowsErrorMessage(GetLastError());
                break;
            }
            if (read == 0) break;
            if (BCryptHashData(hash, buffer.data(), read, 0) < 0) {
                error = L"Falha ao calcular o SHA-256.";
                break;
            }
        }
        if (!error.empty()) break;
        if (BCryptFinishHash(hash, digest.data(), digestSize, 0) < 0) {
            error = L"Falha ao concluir o SHA-256.";
            break;
        }
        hexDigestOut = hexDigest(digest.data(), digest.size());
        ok = true;
    } while (false);

    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
}

std::wstring quoteArgument(std::wstring_view argument) {
    if (argument.empty()) return L"\"\"";
    if (argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos) {
        return std::wstring(argument);
    }
    std::wstring result;
    result.push_back(L'"');
    for (auto it = argument.begin();; ++it) {
        size_t backslashes = 0;
        while (it != argument.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }
        if (it == argument.end()) {
            result.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
        } else {
            result.append(backslashes, L'\\');
            result.push_back(*it);
        }
    }
    result.push_back(L'"');
    return result;
}

} // namespace ds
