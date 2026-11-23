#include "iso.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <cwctype>

namespace ds {
namespace {

constexpr uint32_t kLogicalSectorData = 2048;
constexpr uint64_t kMaxDirectoryBytes = 64ull * 1024 * 1024;
constexpr int kMaxDepth = 16;
constexpr size_t kMaxEntries = 200000;

uint16_t readLe16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

uint32_t readLe32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

std::string normalizeSlashes(std::string text) {
    for (char& ch : text) {
        if (ch == '\\') ch = '/';
    }
    while (!text.empty() && text.front() == '/') text.erase(text.begin());
    return text;
}

std::wstring lowerCopy(std::wstring_view text) {
    std::wstring result(text);
    for (wchar_t& ch : result) ch = static_cast<wchar_t>(std::towlower(ch));
    return result;
}

bool validComponent(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    for (unsigned char ch : name) {
        if (ch < 0x20 || ch == 0x7f) return false;
        if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' ||
            ch == '<' || ch == '>' || ch == '|') {
            return false;
        }
    }
    return true;
}

std::string cleanIsoName(std::string name, bool directory) {
    if (!directory) {
        const size_t version = name.find(';');
        if (version != std::string::npos) name.resize(version);
    }
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) name.pop_back();
    while (!name.empty() && name.front() == ' ') name.erase(name.begin());
    return name;
}

} // namespace

IsoImage::~IsoImage() { close(); }

void IsoImage::close() {
    if (file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
    entries_.clear();
    visitedDirectories_.clear();
    imageBytes_ = 0;
    volumeLabel_.clear();
}

bool IsoImage::isOpen() const { return file_ != INVALID_HANDLE_VALUE; }

bool IsoImage::detectSectorLayout(std::wstring& error) {
    struct Layout {
        uint32_t sector;
        uint32_t offset;
    };
    static constexpr Layout candidates[] = {
        {2048, 0}, {2352, 24}, {2352, 16}, {2336, 8}};
    std::array<uint8_t, 2352> buffer{};
    for (const Layout& candidate : candidates) {
        LARGE_INTEGER offset{};
        offset.QuadPart = static_cast<LONGLONG>(16) * candidate.sector + candidate.offset;
        if (offset.QuadPart + kLogicalSectorData > static_cast<LONGLONG>(imageBytes_)) continue;
        if (!SetFilePointerEx(file_, offset, nullptr, FILE_BEGIN)) continue;
        DWORD read = 0;
        if (!ReadFile(file_, buffer.data(), kLogicalSectorData, &read, nullptr) ||
            read < kLogicalSectorData) {
            continue;
        }
        if (std::memcmp(buffer.data() + 1, "CD001", 5) == 0) {
            sectorSize_ = candidate.sector;
            sectorDataOffset_ = candidate.offset;
            return true;
        }
    }
    error = L"Esta imagem não contém um sistema de arquivos ISO9660 reconhecido. "
            L"Selecione um arquivo .iso da edição americana de PlayStation 2.";
    return false;
}

bool IsoImage::open(const fs::path& imagePath, std::wstring& error) {
    close();
    error.clear();
    file_ = CreateFileW(imagePath.c_str(), GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                        nullptr);
    if (file_ == INVALID_HANDLE_VALUE) {
        error = L"Não foi possível abrir a imagem: " + imagePath.wstring() + L"\n" +
                windowsErrorMessage(GetLastError());
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file_, &size) || size.QuadPart <= 0) {
        error = L"Não foi possível medir a imagem selecionada.";
        close();
        return false;
    }
    imageBytes_ = static_cast<uint64_t>(size.QuadPart);
    if (!detectSectorLayout(error)) {
        close();
        return false;
    }

    std::array<uint8_t, kLogicalSectorData> pvd{};
    if (!readSectorData(16, pvd.data(), error)) {
        close();
        return false;
    }
    if (pvd[0] != 1 || std::memcmp(pvd.data() + 1, "CD001", 5) != 0) {
        error = L"O descritor de volume primário da imagem está inválido.";
        close();
        return false;
    }
    const uint8_t rootRecordLength = pvd[156];
    if (rootRecordLength < 34 || 156 + rootRecordLength > pvd.size()) {
        error = L"O diretório raiz da imagem está inválido.";
        close();
        return false;
    }
    const uint32_t rootLba = readLe32(pvd.data() + 156 + 2);
    const uint64_t rootSize = readLe32(pvd.data() + 156 + 10);
    if (rootSize == 0 || rootSize > kMaxDirectoryBytes) {
        error = L"O diretório raiz da imagem tem um tamanho inesperado.";
        close();
        return false;
    }

    const std::string label(reinterpret_cast<const char*>(pvd.data() + 40), 32);
    volumeLabel_ = trim(toWide(label));

    IsoEntry root;
    root.path.clear();
    root.directory = true;
    root.size = rootSize;
    root.extents.push_back({rootLba, rootSize});
    visitedDirectories_.insert(rootLba);
    if (!parseDirectory(root, "", 0, error)) {
        close();
        return false;
    }
    if (entries_.empty()) {
        error = L"A imagem não contém arquivos legíveis.";
        close();
        return false;
    }
    std::sort(entries_.begin(), entries_.end(),
              [](const IsoEntry& a, const IsoEntry& b) { return a.path < b.path; });
    return true;
}

bool IsoImage::readSectorData(uint32_t lba, uint8_t* buffer, std::wstring& error) const {
    const uint64_t offset = static_cast<uint64_t>(lba) * sectorSize_ + sectorDataOffset_;
    if (offset + kLogicalSectorData > imageBytes_) {
        error = L"A imagem está truncada: setor fora dos limites do arquivo.";
        return false;
    }
    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(file_, position, nullptr, FILE_BEGIN)) {
        error = L"Falha ao posicionar na imagem: " + windowsErrorMessage(GetLastError());
        return false;
    }
    DWORD read = 0;
    if (!ReadFile(file_, buffer, kLogicalSectorData, &read, nullptr) ||
        read != kLogicalSectorData) {
        error = L"Falha ao ler a imagem: " + windowsErrorMessage(GetLastError());
        return false;
    }
    return true;
}

bool IsoImage::parseDirectory(const IsoEntry& directory, const std::string& prefix, int depth,
                              std::wstring& error) {
    if (depth > kMaxDepth) {
        error = L"A árvore de diretórios da imagem é profunda demais.";
        return false;
    }
    if (directory.size == 0) return true;
    if (directory.size > kMaxDirectoryBytes) {
        error = L"Um diretório da imagem excede o tamanho máximo aceito.";
        return false;
    }
    std::vector<uint8_t> data(static_cast<size_t>(directory.size));
    if (!readFileData(directory.extents, 0, data.data(), data.size(), error)) return false;
    return parseDirectoryData(data, prefix, depth, error);
}

bool IsoImage::parseDirectoryData(const std::vector<uint8_t>& data, const std::string& prefix,
                                  int depth, std::wstring& error) {
    size_t position = 0;
    while (position < data.size()) {
        const uint8_t length = data[position];
        if (length == 0) {
            // Records never cross a logical sector; the rest of the sector is padding.
            const size_t next = ((position / kLogicalSectorData) + 1) * kLogicalSectorData;
            if (next <= position) break;
            position = next;
            continue;
        }
        if (length < 34 || position + length > data.size()) {
            error = L"Registro de diretório inválido dentro da imagem.";
            return false;
        }
        const uint8_t* record = data.data() + position;
        const uint8_t flags = record[25];
        const uint32_t extentLba = readLe32(record + 2);
        const uint64_t extentSize = readLe32(record + 10);
        const uint8_t nameLength = record[32];
        if (33u + nameLength > length) {
            error = L"Registro de diretório com nome inválido.";
            return false;
        }
        std::string name(reinterpret_cast<const char*>(record + 33), nameLength);
        position += length;

        if (nameLength == 1 && (static_cast<unsigned char>(name[0]) == 0 ||
                                static_cast<unsigned char>(name[0]) == 1)) {
            continue;
        }
        const bool isDirectory = (flags & 0x02) != 0;
        name = cleanIsoName(name, isDirectory);
        if (name.empty()) continue;
        if (!validComponent(name)) {
            // ISO9660 file names are ASCII; a component with path separators or
            // control characters cannot be extracted safely.
            continue;
        }
        if (entries_.size() >= kMaxEntries) {
            error = L"A imagem contém arquivos demais.";
            return false;
        }

        const std::string path = prefix + name;
        if (isDirectory) {
            IsoEntry child;
            child.path = path;
            child.displayName = toWide(name);
            child.directory = true;
            child.size = extentSize;
            child.extents.push_back({extentLba, extentSize});
            entries_.push_back(child);
            if (extentSize == 0) continue;
            if (!visitedDirectories_.insert(extentLba).second) continue;
            if (!parseDirectory(child, path + "/", depth + 1, error)) return false;
            continue;
        }

        const bool multiExtent = (flags & 0x80) != 0;
        IsoEntry* existing = nullptr;
        if (multiExtent) {
            for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
                if (it->path == path) {
                    existing = &*it;
                    break;
                }
            }
        }
        if (existing) {
            existing->extents.push_back({extentLba, extentSize});
            existing->size += extentSize;
        } else {
            IsoEntry file;
            file.path = path;
            file.displayName = toWide(name);
            file.size = extentSize;
            file.extents.push_back({extentLba, extentSize});
            entries_.push_back(std::move(file));
        }
    }
    return true;
}

bool IsoImage::readFileData(const std::vector<IsoExtent>& extents, uint64_t offset, void* buffer,
                            size_t bytes, std::wstring& error) const {
    uint8_t* destination = static_cast<uint8_t*>(buffer);
    size_t remaining = bytes;
    uint64_t extentStart = 0;
    for (const IsoExtent& extent : extents) {
        if (remaining == 0) break;
        const uint64_t extentEnd = extentStart + extent.size;
        if (offset >= extentEnd) {
            extentStart = extentEnd;
            continue;
        }
        uint64_t local = offset > extentStart ? offset - extentStart : 0;
        size_t toRead = static_cast<size_t>(
            std::min<uint64_t>(remaining, extent.size - local));
        if (sectorSize_ == kLogicalSectorData && sectorDataOffset_ == 0) {
            // Standard images store 2048 user bytes per sector contiguously.
            uint64_t physical = static_cast<uint64_t>(extent.lba) * sectorSize_ + local;
            size_t done = 0;
            while (done < toRead) {
                const size_t chunk = std::min<size_t>(toRead - done, 4u << 20);
                LARGE_INTEGER position{};
                position.QuadPart = static_cast<LONGLONG>(physical + done);
                if (!SetFilePointerEx(file_, position, nullptr, FILE_BEGIN)) {
                    error = L"Falha ao posicionar na imagem: " +
                            windowsErrorMessage(GetLastError());
                    return false;
                }
                DWORD read = 0;
                if (!ReadFile(file_, destination + done, static_cast<DWORD>(chunk), &read,
                              nullptr) ||
                    read == 0) {
                    error = L"Falha ao ler dados da imagem: " +
                            windowsErrorMessage(GetLastError());
                    return false;
                }
                done += read;
                if (read < chunk) break;
            }
            if (done != toRead) {
                error = L"A imagem terminou antes do esperado.";
                return false;
            }
        } else {
            // Raw 2352/2336-byte sectors interleave headers, so copy one sector at a time.
            const size_t requested = toRead;
            while (toRead > 0) {
                const uint64_t logical = static_cast<uint64_t>(extent.lba) * kLogicalSectorData +
                                         local;
                const uint32_t sector = static_cast<uint32_t>(logical / kLogicalSectorData);
                const size_t inSector = static_cast<size_t>(logical % kLogicalSectorData);
                const size_t chunk = std::min<size_t>(toRead, kLogicalSectorData - inSector);
                std::array<uint8_t, kLogicalSectorData> sectorData{};
                if (!readSectorData(sector, sectorData.data(), error)) return false;
                std::memcpy(destination, sectorData.data() + inSector, chunk);
                destination += chunk;
                local += chunk;
                toRead -= chunk;
                remaining -= chunk;
            }
            offset += requested;
            extentStart = extentEnd;
            continue;
        }
        destination += toRead;
        remaining -= toRead;
        offset += toRead;
        extentStart = extentEnd;
    }
    if (remaining != 0) {
        error = L"Leitura além do fim do arquivo na imagem.";
        return false;
    }
    return true;
}

bool IsoImage::readAll(const IsoEntry& entry, std::vector<uint8_t>& out, size_t maxBytes,
                       std::wstring& error) const {
    if (entry.directory) {
        error = L"O item selecionado é um diretório.";
        return false;
    }
    if (entry.size > maxBytes) {
        error = L"Arquivo grande demais para leitura: " + entry.displayName;
        return false;
    }
    out.resize(static_cast<size_t>(entry.size));
    if (entry.size == 0) return true;
    return readFileData(entry.extents, 0, out.data(), out.size(), error);
}

bool IsoImage::readRegion(const IsoEntry& entry, uint64_t offset, void* buffer, size_t bytes,
                          std::wstring& error) const {
    if (entry.directory || offset + bytes > entry.size) {
        error = L"Leitura fora dos limites do arquivo na imagem.";
        return false;
    }
    return readFileData(entry.extents, offset, buffer, bytes, error);
}

bool IsoImage::extract(const IsoEntry& entry, const fs::path& destination,
                       const CancelToken& cancel, const std::function<void(uint64_t)>& onBytes,
                       std::wstring& error) const {
    if (entry.directory) {
        error = L"O item selecionado é um diretório.";
        return false;
    }
    std::error_code ec;
    if (!destination.parent_path().empty()) {
        fs::create_directories(destination.parent_path(), ec);
        if (ec) {
            error = L"Não foi possível criar a pasta " + destination.parent_path().wstring() +
                    L": " + toWide(ec.message());
            return false;
        }
    }
    fs::path part = destination;
    part += L".part";
    HANDLE output = CreateFileW(part.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        error = L"Não foi possível criar " + part.wstring() + L": " +
                windowsErrorMessage(GetLastError());
        return false;
    }

    std::vector<uint8_t> buffer(4u << 20);
    uint64_t offset = 0;
    bool success = true;
    while (offset < entry.size) {
        if (cancel.cancelled()) {
            error = L"Extração cancelada.";
            success = false;
            break;
        }
        const size_t chunk = static_cast<size_t>(std::min<uint64_t>(buffer.size(), entry.size - offset));
        if (!readFileData(entry.extents, offset, buffer.data(), chunk, error)) {
            success = false;
            break;
        }
        DWORD written = 0;
        if (!WriteFile(output, buffer.data(), static_cast<DWORD>(chunk), &written, nullptr) ||
            written != chunk) {
            error = L"Falha ao gravar " + destination.wstring() + L": " +
                    windowsErrorMessage(GetLastError());
            success = false;
            break;
        }
        offset += chunk;
        if (onBytes) onBytes(offset);
    }
    FlushFileBuffers(output);
    CloseHandle(output);
    if (!success) {
        DeleteFileW(part.c_str());
        return false;
    }
    SetFileAttributesW(destination.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (!MoveFileExW(part.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = L"Não foi possível finalizar " + destination.wstring() + L": " +
                windowsErrorMessage(GetLastError());
        DeleteFileW(part.c_str());
        return false;
    }
    return true;
}

const IsoEntry* IsoImage::find(std::wstring_view path) const {
    const std::string normalizedUtf8 = normalizeSlashes(toUtf8(path));
    const std::wstring wanted = lowerCopy(toWide(normalizedUtf8));
    for (const IsoEntry& entry : entries_) {
        if (entry.directory) continue;
        if (lowerCopy(toWide(entry.path)) == wanted) return &entry;
    }
    return nullptr;
}

uint64_t IsoImage::payloadBytes() const {
    uint64_t total = 0;
    for (const IsoEntry& entry : entries_) {
        if (!entry.directory) total += entry.size;
    }
    return total;
}

} // namespace ds
