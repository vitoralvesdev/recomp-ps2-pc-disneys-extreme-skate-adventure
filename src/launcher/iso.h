#pragma once

#include "common.h"

#include <set>

namespace ds {

struct IsoExtent {
    uint32_t lba = 0;
    uint64_t size = 0;
};

struct IsoEntry {
    std::string path; // relative, forward slashes, no leading slash
    std::wstring displayName;
    std::vector<IsoExtent> extents;
    uint64_t size = 0;
    bool directory = false;
};

// Minimal ISO9660 reader for PlayStation 2 DVD images. It understands 2048-byte
// ISO images and the 2352/2336-byte raw sector variants, walks the directory
// tree, and extracts files without external tools.
class IsoImage {
public:
    IsoImage() = default;
    ~IsoImage();
    IsoImage(const IsoImage&) = delete;
    IsoImage& operator=(const IsoImage&) = delete;

    bool open(const fs::path& imagePath, std::wstring& error);
    void close();
    bool isOpen() const;

    const std::vector<IsoEntry>& entries() const { return entries_; }
    const IsoEntry* find(std::wstring_view path) const;

    bool readAll(const IsoEntry& entry, std::vector<uint8_t>& out, size_t maxBytes,
                 std::wstring& error) const;
    bool readRegion(const IsoEntry& entry, uint64_t offset, void* buffer, size_t bytes,
                    std::wstring& error) const;

    // Writes one file atomically (destination.part -> destination). onBytes is
    // called after each chunk so the UI can report real progress.
    bool extract(const IsoEntry& entry, const fs::path& destination, const CancelToken& cancel,
                 const std::function<void(uint64_t)>& onBytes, std::wstring& error) const;

    uint64_t imageBytes() const { return imageBytes_; }
    uint64_t payloadBytes() const;
    std::wstring volumeLabel() const { return volumeLabel_; }
    uint32_t sectorSize() const { return sectorSize_; }
    uint32_t sectorDataOffset() const { return sectorDataOffset_; }

private:
    bool readSectorData(uint32_t lba, uint8_t* buffer, std::wstring& error) const;
    bool readFileData(const std::vector<IsoExtent>& extents, uint64_t offset, void* buffer,
                      size_t bytes, std::wstring& error) const;
    bool parseDirectory(const IsoEntry& directory, const std::string& prefix, int depth,
                        std::wstring& error);
    bool parseDirectoryData(const std::vector<uint8_t>& data, const std::string& prefix,
                            int depth, std::wstring& error);
    bool detectSectorLayout(std::wstring& error);

    HANDLE file_ = INVALID_HANDLE_VALUE;
    uint64_t imageBytes_ = 0;
    uint32_t sectorSize_ = 2048;
    uint32_t sectorDataOffset_ = 0;
    std::wstring volumeLabel_;
    std::vector<IsoEntry> entries_;
    std::set<uint32_t> visitedDirectories_;
};

} // namespace ds
