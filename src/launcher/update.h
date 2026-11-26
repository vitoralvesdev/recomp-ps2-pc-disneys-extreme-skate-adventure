#pragma once

#include "common.h"

namespace ds {

struct ReleaseAsset {
    std::wstring name;
    std::wstring downloadUrl;
    uint64_t size = 0;
};

struct ReleaseInfo {
    std::wstring tag;
    std::wstring title;
    std::wstring notes;
    std::wstring htmlUrl;
    std::wstring publishedAt;
    std::vector<ReleaseAsset> assets;

    const ReleaseAsset* findAssetWithSuffix(std::wstring_view suffix) const;
};

// Queries the GitHub releases API. Never blocks longer than the WinHTTP timeouts.
bool fetchLatestRelease(ReleaseInfo& release, std::wstring& error);

// Numeric comparison of versions such as "v0.4.0" and "0.10.1"; returns <0 when
// left is older. A prerelease suffix sorts before the same release version.
int compareVersions(std::wstring_view left, std::wstring_view right);

std::wstring installerNameForTag(std::wstring_view tag);

// Downloads an installer/component asset to destination with progress.
bool downloadReleaseAsset(const std::wstring& url, const fs::path& destination,
                          std::wstring_view expectedTag, std::wstring_view expectedName,
                          uint64_t expectedSize, const CancelToken& cancel,
                          const std::function<void(uint64_t, uint64_t)>& onProgress,
                          std::wstring& error);

} // namespace ds
