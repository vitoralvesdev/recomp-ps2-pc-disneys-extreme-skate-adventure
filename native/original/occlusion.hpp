#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace disney::original {
struct Point3 { float x,y,z; };
struct Plane { float x,y,z,offset; };

// Layout recovered from SLUS_206.07: 0x70-byte occluder records,
// five planes at +0x10 and an unsigned usage counter at +0x60.
// Unknown fields are deliberately preserved as opaque data.
struct Occluder {
    std::array<std::uint32_t,4> opaqueHeader{};
    std::array<Plane,5> planes{};
    std::uint32_t hitCount=0;
    std::array<std::uint32_t,3> opaqueTail{};
};
static_assert(sizeof(Plane)==0x10);
static_assert(sizeof(Occluder)==0x70);
static_assert(offsetof(Occluder,planes)==0x10);
static_assert(offsetof(Occluder,hitCount)==0x60);

// Port of the ORIGINAL game's scalar branch in FUN_001624c0,
// instructions 0x16254c..0x1625f8. The original also offers a VU0 fast path.
// No console registers, instruction decoder or guest-memory dependency.
bool isOccluded(std::span<Occluder> occluders,Point3 center,float radius,
                std::uint32_t counterIncrement) noexcept;
}
