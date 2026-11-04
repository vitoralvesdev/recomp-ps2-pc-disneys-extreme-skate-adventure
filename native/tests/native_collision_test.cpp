#include "native_collision.h"
#include <iostream>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void write32(std::ostream& file, std::uint32_t value) {
    const char bytes[]{char(value), char(value >> 8), char(value >> 16), char(value >> 24)};
    file.write(bytes, 4);
}
static void write16(std::ostream& file, std::uint16_t value) {
    const char bytes[]{char(value), char(value >> 8)};
    file.write(bytes, 2);
}
static void save(const std::filesystem::path& path, const std::vector<NativeTriangle>& triangles) {
    std::ofstream file(path, std::ios::binary);
    file.write("DSCOL001", 8);
    write32(file, static_cast<std::uint32_t>(triangles.size()));
    for (const auto& triangle : triangles) {
        for (const auto p : {triangle.a, triangle.b, triangle.c})
            for (const auto value : {p.x, p.y, p.z}) write32(file, std::bit_cast<std::uint32_t>(value));
        write16(file, triangle.flags);
        write16(file, triangle.terrain);
        write32(file, triangle.object);
    }
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path path(argv[1]);
    try {
        // Negative cells, a trigger above a solid floor, and a wall at x=-20.
        std::vector<NativeTriangle> triangles{
            {{-100,0,-100}, {-100,0,100}, {100,0,-100}},
            {{-100,10,-100}, {-100,10,100}, {100,10,-100}, {}, 0x10},
            {{-20,0,-100}, {-20,100,-100}, {-20,0,100}}
        };
        save(path, triangles);
        NativeWorld world;
        require(world.load(path), world.error.c_str());
        const auto floor = world.cast({-30,50,-30}, {-30,-50,-30}, 0.5f);
        require(floor.hit && std::abs(floor.point.y) < 1e-4f && floor.normal.y > 0.99f,
                "Solid floor not found behind ignored trigger");
        const auto trigger = world.cast({-30,50,-30}, {-30,-50,-30}, 0.5f, 0);
        require(trigger.hit && std::abs(trigger.point.y - 10) < 1e-4f,
                "Explicit trigger query did not return nearest surface");
        const auto wall = world.cast({-30,25,-30}, {0,25,-30});
        require(wall.hit && std::abs(wall.point.x + 20) < 1e-4f && wall.normal.x < -0.99f,
                "Wall normal must oppose ray direction");
        require(!world.cast({-30,25,-30}, {0,25,-30}, 0.5f).hit,
                "Ground probe accepted a vertical wall");
        require(!world.cast({-30,50,-30}, {-30,20,-30}).hit,
                "Segment hit a surface beyond its endpoint");
        require(!world.cast({0,0,0}, {0,0,0}).hit, "Zero-length ray was accepted");

        // A giant plane uses the overflow list, with a reversed winding.
        triangles = {{{-100000,0,-100000}, {100000,0,-100000}, {-100000,0,100000}}};
        save(path, triangles);
        require(world.load(path), world.error.c_str());
        require(world.cast({-50,10,-50}, {-50,-10,-50}, 0.9f).hit,
                "Overflow triangle or reversed winding missed");
        require(world.cast({-200000,10,-200000}, {200000,-10,200000}).hit,
                "Long diagonal query fallback missed intersection");

        // Failures must preserve the last valid world.
        {
            std::ofstream file(path, std::ios::binary);
            file.write("DSCOL001", 8);
            write32(file, 2'000'001);
        }
        require(!world.load(path), "Oversized count was accepted");
        require(world.cast({-50,10,-50}, {-50,-10,-50}).hit,
                "Failed load destroyed valid collision");
        triangles[0].a.x = std::numeric_limits<float>::quiet_NaN();
        save(path, triangles);
        require(!world.load(path), "Non-finite vertex was accepted");
        {
            std::ofstream file(path, std::ios::binary);
            file.write("DSCOL001", 8);
            write32(file, 1);
        }
        require(!world.load(path), "Truncated triangle was accepted");
        std::filesystem::remove(path);
        std::cout << "Native collision: ground, walls, trigger filtering, winding, grid overflow, long rays and malformed input passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        std::filesystem::remove(path);
        return 1;
    }
}
