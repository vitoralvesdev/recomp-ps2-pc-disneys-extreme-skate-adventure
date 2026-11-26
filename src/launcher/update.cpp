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
constexpr wchar_t kDownloadHost[] = L"github.com";
constexpr std::wstring_view kReleasePathPrefix =
    L"/" DISNEY_LAUNCHER_REPOSITORY_OWNER L"/" DISNEY_LAUNCHER_REPOSITORY_NAME
    L"/releases/download/";
constexpr uint64_t kMaximumInstallerSize = 512ULL * 1024 * 1024;
constexpr std::wstring_view kRedirectHosts[] = {
    L"objects.githubusercontent.com", L"github-releases.githubusercontent.com",
    L"release-assets.githubusercontent.com"};

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

bool configureSecureSession(HINTERNET session, DWORD timeout, std::wstring& error) {
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols,
                          sizeof(protocols)) ||
        !WinHttpSetTimeouts(session, 6000, 6000, 6000, timeout)) {
        error = L"Não foi possível configurar a conexão segura: " +
                windowsErrorMessage(GetLastError());
        return false;
    }
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &redirects,
                          sizeof(redirects))) {
        error = L"Não foi possível bloquear redirecionamentos automáticos: " +
                windowsErrorMessage(GetLastError());
        return false;
    }
    return true;
}

bool parseContentLength(const std::wstring& text, uint64_t& value) {
    if (text.empty()) return false;
    for (const wchar_t ch : text) {
        if (ch < L'0' || ch > L'9') return false;
    }
    try {
        size_t consumed = 0;
        value = std::stoull(text, &consumed, 10);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
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

bool isAllowedRedirectHost(std::wstring_view host) {
    for (const std::wstring_view candidate : kRedirectHosts) {
        if (iequals(host, candidate)) return true;
    }
    return false;
}

bool isReleaseTagCharacter(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
           (ch >= L'0' && ch <= L'9') || ch == L'.' || ch == L'-' || ch == L'_';
}

bool parseUrl(const std::wstring& url, URL_COMPONENTS& components, std::wstring& host,
              std::wstring& path, bool allowRedirect, std::wstring_view expectedTag,
              std::wstring_view expectedName, std::wstring& error) {
    std::memset(&components, 0, sizeof(components));
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    components.dwUserNameLength = static_cast<DWORD>(-1);
    components.dwPasswordLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components)) {
        error = L"Endereço de download inválido.";
        return false;
    }
    if (components.nScheme != INTERNET_SCHEME_HTTPS) {
        error = L"Somente downloads HTTPS são aceitos.";
        return false;
    }
    if (components.nPort != INTERNET_DEFAULT_HTTPS_PORT ||
        components.dwUserNameLength > 0 || components.dwPasswordLength > 0) {
        error = L"Porta ou credenciais não são permitidas no download.";
        return false;
    }
    host.assign(components.lpszHostName, components.dwHostNameLength);
    if (!allowRedirect && !iequals(host, kDownloadHost)) {
        error = L"O download só é permitido no host oficial do GitHub.";
        return false;
    }
    if (allowRedirect && !isAllowedRedirectHost(host)) {
        error = L"O redirecionamento aponta para um host não autorizado.";
        return false;
    }
    path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.lpszExtraInfo && components.dwExtraInfoLength) {
        std::wstring extra(components.lpszExtraInfo, components.dwExtraInfoLength);
        if (extra.find(L'#') != std::wstring::npos) {
            error = L"Fragmentos não são permitidos no endereço de download.";
            return false;
        }
        if (!allowRedirect) {
            error = L"Parâmetros não são permitidos no endereço do instalador.";
            return false;
        }
        path += extra;
    }
    if (allowRedirect) {
        if (path.empty() || path.front() != L'/') {
            error = L"Redirecionamento de download inválido.";
            return false;
        }
        return true;
    }
    if (expectedTag.empty() || expectedName.empty() ||
        path.size() <= kReleasePathPrefix.size() + expectedTag.size() + expectedName.size() + 2 ||
        path.compare(0, kReleasePathPrefix.size(), kReleasePathPrefix) != 0) {
        error = L"O instalador não pertence ao repositório oficial.";
        return false;
    }
    const size_t assetOffset = path.size() - expectedName.size();
    if (path[assetOffset - 1] != L'/' ||
        path.compare(assetOffset, expectedName.size(), expectedName) != 0) {
        error = L"O nome do instalador não é o asset oficial esperado.";
        return false;
    }
    const std::wstring_view tag =
        std::wstring_view(path).substr(kReleasePathPrefix.size(),
                                       assetOffset - kReleasePathPrefix.size() - 1);
    if (tag != expectedTag || !std::all_of(tag.begin(), tag.end(), isReleaseTagCharacter)) {
        error = L"A tag do release do instalador é inválida.";
        return false;
    }
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

std::wstring installerNameForTag(std::wstring_view tag) {
    std::wstring version(trim(tag));
    if (!version.empty() && (version.front() == L'v' || version.front() == L'V')) {
        version.erase(version.begin());
    }
    if (version.empty() || version.size() > 32) return {};
    bool digit = false;
    for (const wchar_t ch : version) {
        if (ch >= L'0' && ch <= L'9') {
            digit = true;
        } else if (ch == L'.' && digit) {
            digit = false;
        } else {
            return {};
        }
    }
    if (!digit) return {};
    return L"DisneySkateSetup-" + version + L".exe";
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
    if (!configureSecureSession(session.handle, 15000, error)) return false;
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
                          std::wstring_view expectedTag, std::wstring_view expectedName,
                          uint64_t expectedSize, const CancelToken& cancel,
                          const std::function<void(uint64_t, uint64_t)>& onProgress,
                          std::wstring& error) {
    error.clear();
    if (expectedSize == 0 || expectedSize > kMaximumInstallerSize) {
        error = L"O tamanho do instalador informado pelo release é inválido.";
        return false;
    }
    std::wstring currentUrl = url;
    for (DWORD redirectCount = 0;; ++redirectCount) {
        URL_COMPONENTS components{};
        std::wstring host;
        std::wstring path;
        if (!parseUrl(currentUrl, components, host, path, redirectCount != 0, expectedTag,
                      expectedName, error)) {
            return false;
        }

        InternetHandle session;
        session.handle = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session.handle) {
            error = L"Não foi possível iniciar o download.";
            return false;
        }
        if (!configureSecureSession(session.handle, 30000, error)) return false;
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
                               WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
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
        if (status >= 300 && status < 400 && status != 304) {
            if (redirectCount >= 3) {
                error = L"O download excedeu o limite de redirecionamentos.";
                return false;
            }
            currentUrl = queryStringHeader(request.handle, WINHTTP_QUERY_LOCATION);
            if (currentUrl.empty()) {
                error = L"O servidor de download não informou o redirecionamento.";
                return false;
            }
            continue;
        }
        if (status != 200) {
            error = L"O servidor de download respondeu com HTTP " + std::to_wstring(status) + L".";
            return false;
        }
        const std::wstring lengthHeader =
            queryStringHeader(request.handle, WINHTTP_QUERY_CONTENT_LENGTH);
        if (!lengthHeader.empty()) {
            uint64_t advertisedSize = 0;
            if (!parseContentLength(lengthHeader, advertisedSize) ||
                advertisedSize != expectedSize) {
                error = L"O tamanho informado pelo servidor não corresponde ao instalador esperado.";
                return false;
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
            if (read == 0 || read > expectedSize - completed) {
                error = L"O instalador recebido excede ou não corresponde ao tamanho esperado.";
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
            if (onProgress) onProgress(completed, expectedSize);
        }
        CloseHandle(file);
        if (!success) {
            DeleteFileW(part.c_str());
            return false;
        }
        if (completed != expectedSize) {
            error = L"O instalador recebido não corresponde ao tamanho esperado.";
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
        if (onProgress) onProgress(expectedSize, expectedSize);
        return true;
    }
}

} // namespace ds
