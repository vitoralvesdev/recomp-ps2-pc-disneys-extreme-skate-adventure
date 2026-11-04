#pragma once

// Native PC collision over converted triangles. No console executable or
// console hardware state is involved. DSCOL001 uses little-endian fields.
#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct NativeTriangle {
    Vector3 a{}, b{}, c{}, normal{};
    std::uint16_t flags = 0, terrain = 0;
    std::uint32_t object = 0;
};

struct NativeHit {
    bool hit = false;
    float fraction = 1.0f;
    Vector3 point{}, normal{};
    std::uint16_t flags = 0;
};

class NativeWorld {
public:
    std::vector<NativeTriangle> triangles;
    std::string error;

    // The count is u32; each record is xyz * 3, flags u16, terrain u16,
    // object checksum u32, totaling 44 bytes. Failed loads preserve the world.
    bool load(const std::filesystem::path& path) {
        try {
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if (!input) throw std::runtime_error("Cannot open collision data");
            const auto end = input.tellg();
            if (end < std::streampos(12))
                throw std::runtime_error("Collision header is truncated");
            const auto size = static_cast<std::uint64_t>(end);
            input.seekg(0);
            std::array<char, 8> magic{};
            input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
            if (std::memcmp(magic.data(), "DSCOL001", 8) != 0)
                throw std::runtime_error("Unsupported collision format");
            const auto count = read_u32(input);
            constexpr std::uint32_t maxTriangles = 2'000'000;
            if (count == 0 || count > maxTriangles)
                throw std::runtime_error("Invalid collision triangle count");
            if (size != 12ull + 44ull * count)
                throw std::runtime_error("Collision count does not match file size");

            NativeWorld next;
            next.triangles.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i) {
                NativeTriangle triangle;
                triangle.a = read_vector(input);
                triangle.b = read_vector(input);
                triangle.c = read_vector(input);
                triangle.flags = read_u16(input);
                triangle.terrain = read_u16(input);
                triangle.object = read_u32(input);
                const auto normal = Vector3CrossProduct(
                    Vector3Subtract(triangle.b, triangle.a),
                    Vector3Subtract(triangle.c, triangle.a));
                const float length = Vector3Length(normal);
                // Exported meshes may contain collapsed strip triangles.
                if (!std::isfinite(length))
                    throw std::runtime_error("Non-finite collision normal");
                if (length <= 1e-7f) continue;
                triangle.normal = Vector3Scale(normal, 1.0f / length);
                next.triangles.push_back(triangle);
            }
            if (next.triangles.empty())
                throw std::runtime_error("Collision contains no usable triangles");
            next.build_grid();
            *this = std::move(next);
            return true;
        } catch (const std::exception& exception) {
            error = exception.what();
            return false;
        }
    }

    // Normals face the ray's start. A downward ground probe consequently
    // returns an upward normal regardless of exported triangle winding.
    // Bit 0x10 is the engine's non-collidable/trigger-only surface flag.
    NativeHit cast(Vector3 from, Vector3 to, float minNormalY = -2.0f,
                   std::uint16_t ignoredFlags = 0x10) const {
        NativeHit result;
        if (!valid_vector(from) || !valid_vector(to) ||
            !std::isfinite(minNormalY) || triangles.empty()) return result;
        const Vector3 direction = Vector3Subtract(to, from);
        if (Vector3LengthSqr(direction) < 1e-12f) return result;

        const auto test = [&](std::uint32_t index) {
            const auto& triangle = triangles[index];
            if ((triangle.flags & ignoredFlags) != 0) return;
            Vector3 normal = triangle.normal;
            if (Vector3DotProduct(normal, direction) > 0.0f)
                normal = Vector3Negate(normal);
            if (normal.y < minNormalY) return;
            const double fraction = intersect(from, direction, triangle);
            if (fraction < 0.0 || fraction > result.fraction ||
                (result.hit && fraction == result.fraction)) return;
            result.hit = true;
            result.fraction = static_cast<float>(fraction);
            result.point = Vector3Add(from, Vector3Scale(direction, result.fraction));
            result.normal = normal;
            result.flags = triangle.flags;
        };

        const auto x0 = cell(std::min(from.x, to.x));
        const auto x1 = cell(std::max(from.x, to.x));
        const auto z0 = cell(std::min(from.z, to.z));
        const auto z1 = cell(std::max(from.z, to.z));
        const auto cells = (static_cast<std::uint64_t>(x1 - x0) + 1) *
                           (static_cast<std::uint64_t>(z1 - z0) + 1);
        // Very long diagonal diagnostic rays must not enumerate huge areas.
        if (cells > 16'384 || marks_.size() != triangles.size()) {
            for (std::uint32_t i = 0; i < triangles.size(); ++i) test(i);
            return result;
        }
        if (++query_ == 0) {
            std::fill(marks_.begin(), marks_.end(), 0);
            query_ = 1;
        }
        const auto once = [&](std::uint32_t index) {
            if (marks_[index] == query_) return;
            marks_[index] = query_;
            test(index);
        };
        for (std::int32_t z = z0; z <= z1; ++z) {
            for (std::int32_t x = x0; x <= x1; ++x) {
                const auto found = grid_.find(key(x, z));
                if (found != grid_.end())
                    for (const auto index : found->second) once(index);
            }
        }
        for (const auto index : large_) once(index);
        return result;
    }

private:
    static constexpr float cellSize = 512.0f;
    static constexpr float coordinateLimit = 1e9f;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> grid_;
    std::vector<std::uint32_t> large_;
    // Main-thread query cache; separate NativeWorld instances are independent.
    mutable std::vector<std::uint32_t> marks_;
    mutable std::uint32_t query_ = 0;

    static bool valid_vector(Vector3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) &&
               std::isfinite(value.z) && std::abs(value.x) <= coordinateLimit &&
               std::abs(value.y) <= coordinateLimit && std::abs(value.z) <= coordinateLimit;
    }
    static std::int32_t cell(float value) {
        return static_cast<std::int32_t>(std::floor(value / cellSize));
    }
    static std::uint64_t key(std::int32_t x, std::int32_t z) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) |
               static_cast<std::uint32_t>(z);
    }
    static std::uint16_t read_u16(std::istream& input) {
        unsigned char bytes[2]{};
        if (!input.read(reinterpret_cast<char*>(bytes), 2))
            throw std::runtime_error("Collision data is truncated");
        return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
    }
    static std::uint32_t read_u32(std::istream& input) {
        unsigned char bytes[4]{};
        if (!input.read(reinterpret_cast<char*>(bytes), 4))
            throw std::runtime_error("Collision data is truncated");
        return static_cast<std::uint32_t>(bytes[0]) |
               (static_cast<std::uint32_t>(bytes[1]) << 8) |
               (static_cast<std::uint32_t>(bytes[2]) << 16) |
               (static_cast<std::uint32_t>(bytes[3]) << 24);
    }
    static Vector3 read_vector(std::istream& input) {
        const Vector3 result{std::bit_cast<float>(read_u32(input)),
                             std::bit_cast<float>(read_u32(input)),
                             std::bit_cast<float>(read_u32(input))};
        if (!valid_vector(result))
            throw std::runtime_error("Invalid collision vertex");
        return result;
    }
    void build_grid() {
        marks_.assign(triangles.size(), 0);
        std::uint64_t references = 0;
        for (std::uint32_t i = 0; i < triangles.size(); ++i) {
            const auto& t = triangles[i];
            const auto x0 = cell(std::min({t.a.x, t.b.x, t.c.x}));
            const auto x1 = cell(std::max({t.a.x, t.b.x, t.c.x}));
            const auto z0 = cell(std::min({t.a.z, t.b.z, t.c.z}));
            const auto z1 = cell(std::max({t.a.z, t.b.z, t.c.z}));
            const auto cells = (static_cast<std::uint64_t>(x1 - x0) + 1) *
                               (static_cast<std::uint64_t>(z1 - z0) + 1);
            if (cells > 2'048 || references + cells > 8'000'000) {
                large_.push_back(i);
                continue;
            }
            references += cells;
            for (std::int32_t z = z0; z <= z1; ++z)
                for (std::int32_t x = x0; x <= x1; ++x)
                    grid_[key(x, z)].push_back(i);
        }
    }
    static double intersect(Vector3 from, Vector3 direction, const NativeTriangle& t) {
        // Double intermediates reduce cracks when large level coordinates
        // and very small triangles occur together. Both windings are accepted.
        const double e1x = double(t.b.x) - t.a.x;
        const double e1y = double(t.b.y) - t.a.y;
        const double e1z = double(t.b.z) - t.a.z;
        const double e2x = double(t.c.x) - t.a.x;
        const double e2y = double(t.c.y) - t.a.y;
        const double e2z = double(t.c.z) - t.a.z;
        const double px = double(direction.y) * e2z - double(direction.z) * e2y;
        const double py = double(direction.z) * e2x - double(direction.x) * e2z;
        const double pz = double(direction.x) * e2y - double(direction.y) * e2x;
        const double determinant = e1x * px + e1y * py + e1z * pz;
        if (std::abs(determinant) < 1e-10) return -1.0;
        const double sx = double(from.x) - t.a.x;
        const double sy = double(from.y) - t.a.y;
        const double sz = double(from.z) - t.a.z;
        const double inverse = 1.0 / determinant;
        const double u = (sx * px + sy * py + sz * pz) * inverse;
        constexpr double epsilon = 1e-7;
        if (u < -epsilon || u > 1.0 + epsilon) return -1.0;
        const double qx = sy * e1z - sz * e1y;
        const double qy = sz * e1x - sx * e1z;
        const double qz = sx * e1y - sy * e1x;
        const double v = (double(direction.x) * qx + double(direction.y) * qy +
                          double(direction.z) * qz) * inverse;
        if (v < -epsilon || u + v > 1.0 + epsilon) return -1.0;
        const double fraction = (e2x * qx + e2y * qy + e2z * qz) * inverse;
        if (fraction < -epsilon || fraction > 1.0 + epsilon) return -1.0;
        return std::clamp(fraction, 0.0, 1.0);
    }
};
