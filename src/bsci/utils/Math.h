#pragma once

#include <cmath>

#include <mc/deps/core/math/Vec2.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/levelgen/structure/BoundingBox.h>
#include <mc/world/phys/AABB.h>

namespace bsci {

inline std::pair<Vec3, Vec3> branchlessONB(Vec3 const& n) {
    float const sign = std::copysign(1.0f, n.z);
    float const a    = -1.0f / (sign + n.z);
    float const b    = n.x * n.y * a;
    return {
        {1.0f + sign * n.x * n.x * a, sign * b,             -sign * n.x},
        {b,                           sign + n.y * n.y * a, -n.y       }
    };
}

} // namespace bsci