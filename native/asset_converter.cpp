#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;

static std::uint32_t read32(const Bytes& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4)
        throw std::runtime_error("Truncated asset header");
    return std::uint32_t(data[offset]) | (std::uint32_t(data[offset+1]) << 8) |
           (std::uint32_t(data[offset+2]) << 16) | (std::uint32_t(data[offset+3]) << 24);
}
static std::uint16_t read16(const Bytes& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 2)
        throw std::runtime_error("Truncated pixel data");
    return std::uint16_t(data[offset] | (std::uint16_t(data[offset+1]) << 8));
}
static Bytes readFile(const fs::path& path, std::size_t limit) {
    const auto size = fs::file_size(path);
    if (size > limit) throw std::runtime_error("Input exceeds expected size");
    Bytes data(static_cast<std::size_t>(size));
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
        throw std::runtime_error("Cannot read input file");
    return data;
}
static std::string normalized(std::string name) {
    for (char& c : name) {
        if (c == '/') c = '\\';
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    }
    return name;
}
static Bytes loadLoadingAsset(const fs::path& extractedDirectory) {
    const Bytes hed = readFile(extractedDirectory / "SKATE4.HED", 16 * 1024 * 1024);
    for (std::size_t at = 0; at + 8 <= hed.size();) {
        const auto offset = read32(hed, at);
        if (offset == 0xffffffffu) break;
        const auto size = read32(hed, at+4);
        const auto end = std::find(hed.begin() + static_cast<std::ptrdiff_t>(at+8), hed.end(), 0);
        if (end == hed.end()) throw std::runtime_error("Unterminated HED filename");
        const std::string name(hed.begin() + static_cast<std::ptrdiff_t>(at+8), end);
        at = (static_cast<std::size_t>(end - hed.begin()) + 4u) & ~std::size_t(3);
        if (normalized(name) != "\\images\\loadscrn.img.ps2") continue;
        const fs::path wad = extractedDirectory / "SKATE4.WAD";
        if (size > 64u*1024u*1024u || std::uint64_t(offset)+size > fs::file_size(wad))
            throw std::runtime_error("Asset range is outside SKATE4.WAD");
        std::ifstream file(wad, std::ios::binary);
        file.seekg(offset);
        Bytes data(size);
        if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size)))
            throw std::runtime_error("Cannot read loading image");
        return data;
    }
    throw std::runtime_error("Loading image is absent from SKATE4.HED");
}
static void append16(Bytes& bytes, std::uint16_t v) {
    bytes.push_back(std::uint8_t(v)); bytes.push_back(std::uint8_t(v >> 8));
}
static void append32(Bytes& bytes, std::uint32_t v) {
    append16(bytes, std::uint16_t(v)); append16(bytes, std::uint16_t(v >> 16));
}
static void convertLoadingImage(const fs::path& extractedDirectory, const fs::path& output) {
    const Bytes source = loadLoadingAsset(extractedDirectory);
    if (source.size() < 32 || read32(source, 0) != 2 || read32(source, 16) != 2)
        throw std::runtime_error("Unsupported loading image version or pixel format");
    const std::uint32_t width = read16(source, 28), height = read16(source, 30);
    const std::uint64_t pixelCount = std::uint64_t(width)*height;
    if (!width || !height || width > 8192 || height > 8192 || 32u+pixelCount*2u != source.size())
        throw std::runtime_error("Invalid loading image dimensions or payload size");
    const std::uint32_t rowBytes = (width*3u+3u) & ~3u;
    const std::uint32_t bitmapBytes = rowBytes*height;
    Bytes bitmap;
    bitmap.reserve(54u+bitmapBytes);
    append16(bitmap, 0x4d42); append32(bitmap, 54u+bitmapBytes);
    append32(bitmap, 0); append32(bitmap, 54);
    append32(bitmap, 40); append32(bitmap, width); append32(bitmap, height);
    append16(bitmap, 1); append16(bitmap, 24); append32(bitmap, 0);
    append32(bitmap, bitmapBytes); append32(bitmap, 2835); append32(bitmap, 2835);
    append32(bitmap, 0); append32(bitmap, 0);
    // This asset stores the bottom row first, matching a positive-height BMP.
    auto expand5 = [](std::uint16_t v) { return std::uint8_t((v << 3) | (v >> 2)); };
    for (std::uint32_t y=0; y<height; ++y) {
        for (std::uint32_t x=0; x<width; ++x) {
            const auto pixel = read16(source, 32u+2u*(std::size_t(y)*width+x));
            bitmap.push_back(expand5((pixel >> 10) & 31u));
            bitmap.push_back(expand5((pixel >> 5) & 31u));
            bitmap.push_back(expand5(pixel & 31u));
        }
        bitmap.insert(bitmap.end(), rowBytes-width*3u, 0);
    }
    if (!output.parent_path().empty()) fs::create_directories(output.parent_path());
    std::ofstream file(output, std::ios::binary);
    if (!file.write(reinterpret_cast<const char*>(bitmap.data()), static_cast<std::streamsize>(bitmap.size())))
        throw std::runtime_error("Cannot write native asset");
    std::cout << "Converted loading image to standard BMP: " << width << 'x' << height
              << ", " << bitmap.size() << " bytes. No guest code executed.\n";
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    try {
        if (argc != 3) {
            std::cerr << "Usage: DisneyAssetConverter <original-extracted-directory> <output.bmp>\n";
            return 2;
        }
        convertLoadingImage(fs::path(argv[1]), fs::path(argv[2]));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Conversion failed: " << error.what() << '\n';
        return 1;
    }
}
