#include "occlusion.hpp"

namespace disney::original {
bool isOccluded(std::span<Occluder> occluders,Point3 center,float radius,
                std::uint32_t counterIncrement) noexcept {
    const float negativeRadius=-radius;
    for(auto& occluder:occluders) {
        bool rejected=false;
        for(const auto& plane:occluder.planes) {
            // Preserve the original three MUL.S and three ADD.S ordering.
            // Do not fuse this into FMA or reassociate the additions.
            const float x=plane.x*center.x;
            const float y=plane.y*center.y;
            const float z=plane.z*center.z;
            const float xy=x+y;
            const float xyz=xy+z;
            const float distance=xyz+plane.offset;
            if(negativeRadius<=distance) { rejected=true;break; }
        }
        if(!rejected) {
            // Original ADDU is a modulo-2^32 counter update.
            occluder.hitCount+=counterIncrement;
            return true;
        }
    }
    return false;
}
}
