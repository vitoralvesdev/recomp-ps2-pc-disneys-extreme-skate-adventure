#include "update.h"

#include "json.h"
#include "version.h"

#include <winhttp.h>

#include <algorithm>
#include <cwctype>
#include <cstring>

namespace ds {
namespace {

constexpr wchar_t kApiHost[] = L"api.github.com";
constexpr wchar_t kApiPath[] =
    L"/repos/" DISNEY_LAUNCHER_REPOSITORY_OWNER L"/" DISNEY_LAUNCHER_REPOSITORY_NAME
    L"/releases/latest";
constexpr wchar_t kUserAgent[] = L"DisneySkateLauncher/" DISNEY_LAUNCHER_VERSION_STRING
    L" (Windows)";

struct InternetHandle {
    HINTERNET handle = nullptr;
    ~InternetHandle() {
        if (handle) WinHttpCloseHandle(handle);
    }
};

std::wstring queryStringHeader(HINTERNET request, DWORD header) {
    DWORD size = 0;
    WinHttpQueryHeaders(request, header, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size,
                        WINHTTP_NO_HEADER_INDEX);
    if (size == 0) return {};
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(request, header, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &size,
                             WINHTTP_NO_HEADER_INDEX)) {
        return {};
    }
    if (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}

bool readResponse(HINTERNET request, std::string& body, std::wstring& error) {
    body.clear();
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            error = L"Falha ao consultar a resposta da atualização: " +
                    windowsErrorMessage(GetLastError());
            return false;
        }
        if (available == 0) break;
        const DWORD chunk = std::min<DWORD>(available, 64 * 1024);
        const size_t oldSize = body.size();
        body.resize(oldSize + chunk);
        DWORD read = 0;
        if (!WinHttpReadData(request, body.data() + oldSize, chunk, &read)) {
            error = L"Falha ao ler a resposta da atualização: " +
                    windowsErrorMessage(GetLastError());
            return false;
        }
        body.resize(oldSize + read);
        if (body.size() > 4 * 1024 * 1024) {
            error = L"A resposta do servidor de atualizações é grande demais.";
            return false;
        }
    }
    return true;
}

bool parseRelease(const Json& root, ReleaseInfo& release, std::wstring& error) {
    if (!root.isObject()) {
        error = L"Resposta de atualização inválida.";
        return false;
    }
    release.tag = toWide(root.stringOr("tag_name"));
    release.title = toWide(root.stringOr("name"));
    release.notes = toWide(root.stringOr("body"));
    release.htmlUrl = toWide(root.stringOr("html_url"));
    release.publishedAt = toWide(root.stringOr("published_at"));
    const Json* assets = root.get("assets");
    if (assets && assets->isArray()) {
        for (const Json& asset : assets->array) {
            ReleaseAsset item;
            item.name = toWide(asset.stringOr("name"));
            item.downloadUrl = toWide(asset.stringOr("browser_download_url"));
            item.size = static_cast<uint64_t>(asset.numberOr("size", 0));
            if (!item.name.empty() && !item.downloadUrl.empty()) {
                release.assets.push_back(std::move(item));
            }
        }
    }
    if (release.tag.empty()) {
        error = L"A resposta não informa a versão da atualização.";
        return false;
    }
    return true;
}

std::wstring lowerText(std::wstring_view text) {
    std::wstring result(text);
    for (wchar_t& ch : result) ch = static_cast<wchar_t>(std::towlower(ch));
    return result;
}

bool isPrerelease(const std::wstring& version) {
    const size_t dash = version.find(L'-');
    return dash != std::wstring::npos && dash + 1 < version.size();
}

int compareVersionImpl(std::wstring_view leftText, std::wstring_view rightText) {
    std::wstring left = lowerText(trim(leftText));
    std::wstring right = lowerText(trim(rightText));
    if (!left.empty() && left.front() == L'v') left.erase(left.begin());
    if (!right.empty() && right.front() == L'v') right.erase(right.begin());
    const bool leftPre = isPrerelease(left);
    const bool rightPre = isPrerelease(right);
    const std::wstring leftCore = left.substr(0, left.find(L'-'));
    const std::wstring rightCore = right.substr(0, right.find(L'-'));

    auto split = [](const std::wstring& text) {
        std::vector<int> parts;
        size_t start = 0;
        while (start <= text.size()) {
            const size_t dot = text.find(L'.', start);
            const std::wstring token =
                text.substr(start, dot == std::wstring::npos ? std::wstring::npos : dot - start);
            int value = 0;
            try {
                value = token.empty() ? 0 : std::stoi(token);
            } catch (...) {
                value = 0;
            }
            parts.push_back(value);
            if (dot == std::wstring::npos) break;
            start = dot + 1;
        }
        return parts;
    };
    const std::vector<int> leftParts = split(leftCore);
    const std::vector<int> rightParts = split(rightCore);
    const size_t count = std::max(leftParts.size(), rightParts.size());
    for (size_t i = 0; i < count; ++i) {
        const int a = i < leftParts.size() ? leftParts[i] : 0;
        const int b = i < rightParts.size() ? rightParts[i] : 0;
        if (a != b) return a < b ? -1 : 1;
    }
    if (leftPre != rightPre) return leftPre ? -1 : 1;
    if (leftPre && rightPre) {
        const std::wstring leftSuffix = left.substr(left.find(L'-') + 1);
        const std::wstring rightSuffix = right.substr(right.find(L'-') + 1);
        if (leftSuffix != rightSuffix) return leftSuffix < rightSuffix ? -1 : 1;
    }
    return 0;
}

bool parseUrl(const std::wstring& url, URL_COMPONENTS& components, std::wstring& host,
              std::wstring& path, bool& secure, std::wstring& error) {
    std::memset(&components, 0, sizeof(components));
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components)) {
        error = L"Endereço de download inválido.";
        return false;
    }
    if (components.nScheme != INTERNET_SCHEME_HTTP &&
        components.nScheme != INTERNET_SCHEME_HTTPS) {
        error = L"Somente downloads HTTP/HTTPS são aceitos.";
        return false;
    }
    host.assign(components.lpszHostName, components.dwHostNameLength);
    path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.lpszExtraInfo && components.dwExtraInfoLength) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    return true;
}

} // namespace

const ReleaseAsset* ReleaseInfo::findAssetWithSuffix(std::wstring_view suffix) const {
    const std::wstring wanted = lowerText(suffix);
    for (const ReleaseAsset& asset : assets) {
        const std::wstring name = lowerText(asset.name);
        if (name.size() >= wanted.size() &&
            name.compare(name.size() - wanted.size(), wanted.size(), wanted) == 0) {
            return &asset;
        }
    }
    return nullptr;
}

int compareVersions(std::wstring_view left, std::wstring_view right) {
    return compareVersionImpl(left, right);
}

bool fetchLatestRelease(ReleaseInfo& release, std::wstring& error) {
    release = ReleaseInfo{};
    error.clear();
    InternetHandle session;
    session.handle = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.handle) {
        error = L"Não foi possível iniciar o WinHTTP: " + windowsErrorMessage(GetLastError());
        return false;
    }
    WinHttpSetTimeouts(session.handle, 6000, 6000, 6000, 15000);
    InternetHandle connection;
    connection.handle = WinHttpConnect(session.handle, kApiHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection.handle) {
        error = L"Não foi possível conectar ao GitHub: " + windowsErrorMessage(GetLastError());
        return false;
    }
    InternetHandle request;
    request.handle = WinHttpOpenRequest(connection.handle, L"GET", kApiPath, nullptr,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (!request.handle) {
        error = L"Não foi possível criar a consulta de atualização.";
        return false;
    }
    const wchar_t headers[] =
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n";
    if (!WinHttpSendRequest(request.handle, headers, static_cast<DWORD>(-1),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.handle, nullptr)) {
        error = L"Não foi possível consultar o GitHub: " +
                windowsErrorMessage(GetLastError());
        return false;
    }
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                        WINHTTP_NO_HEADER_INDEX);
    if (status == 404) {
        error = L"Ainda não há versões publicadas no GitHub.";
        return false;
    }
    if (status != 200) {
        error = L"O GitHub respondeu com HTTP " + std::to_wstring(status) + L".";
        return false;
    }
    std::string body;
    if (!readResponse(request.handle, body, error)) return false;
    std::string parseError;
    const auto json = Json::parse(body, &parseError);
    if (!json) {
        error = L"Resposta inválida do GitHub: " + toWide(parseError);
        return false;
    }
    return parseRelease(*json, release, error);
}

bool downloadReleaseAsset(const std::wstring& url, const fs::path& destination,
                          const CancelToken& cancel,
                          const std::function<void(uint64_t, uint64_t)>& onProgress,
                          std::wstring& error) {
    error.clear();
    URL_COMPONENTS components{};
    std::wstring host;
    std::wstring path;
    bool secure = false;
    if (!parseUrl(url, components, host, path, secure, error)) return false;

    InternetHandle session;
    session.handle = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.handle) {
        error = L"Não foi possível iniciar o download.";
        return false;
    }
    WinHttpSetTimeouts(session.handle, 6000, 6000, 6000, 30000);
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(session.handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirects,
                     sizeof(redirects));
    InternetHandle connection;
    connection.handle = WinHttpConnect(session.handle, host.c_str(),
                                       static_cast<INTERNET_PORT>(components.nPort), 0);
    if (!connection.handle) {
        error = L"Não foi possível conectar ao servidor de download.";
        return false;
    }
    InternetHandle request;
    request.handle =
        WinHttpOpenRequest(connection.handle, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                           WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request.handle) {
        error = L"Não foi possível criar o pedido de download.";
        return false;
    }
    if (!WinHttpSendRequest(request.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.handle, nullptr)) {
        error = L"Falha no download: " + windowsErrorMessage(GetLastError());
        return false;
    }
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                        WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        error = L"O servidor de download respondeu com HTTP " + std::to_wstring(status) + L".";
        return false;
    }
    const std::wstring lengthHeader = queryStringHeader(request.handle, WINHTTP_QUERY_CONTENT_LENGTH);
    uint64_t total = 0;
    if (!lengthHeader.empty()) {
        try {
            total = std::stoull(lengthHeader);
        } catch (...) {
            total = 0;
        }
    }

    std::error_code ec;
    if (!destination.parent_path().empty()) fs::create_directories(destination.parent_path(), ec);
    fs::path part = destination;
    part += L".part";
    HANDLE file = CreateFileW(part.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Não foi possível criar " + part.wstring() + L": " +
                windowsErrorMessage(GetLastError());
        return false;
    }
    uint64_t completed = 0;
    bool success = true;
    std::vector<uint8_t> buffer(256 * 1024);
    for (;;) {
        if (cancel.cancelled()) {
            error = L"Download cancelado.";
            success = false;
            break;
        }
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.handle, &available)) {
            error = L"Falha ao receber o download: " + windowsErrorMessage(GetLastError());
            success = false;
            break;
        }
        if (available == 0) break;
        DWORD read = 0;
        const DWORD chunk = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
        if (!WinHttpReadData(request.handle, buffer.data(), chunk, &read)) {
            error = L"Falha ao ler o download: " + windowsErrorMessage(GetLastError());
            success = false;
            break;
        }
        DWORD written = 0;
        if (!WriteFile(file, buffer.data(), read, &written, nullptr) || written != read) {
            error = L"Falha ao gravar o download: " + windowsErrorMessage(GetLastError());
            success = false;
            break;
        }
        completed += read;
        if (onProgress) onProgress(completed, total);
    }
    CloseHandle(file);
    if (!success) {
        DeleteFileW(part.c_str());
        return false;
    }
    if (!MoveFileExW(part.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = L"Não foi possível concluir o download: " +
                windowsErrorMessage(GetLastError());
        DeleteFileW(part.c_str());
        return false;
    }
    if (onProgress) onProgress(completed, total ? total : completed);
    return true;
}

} // namespace ds
