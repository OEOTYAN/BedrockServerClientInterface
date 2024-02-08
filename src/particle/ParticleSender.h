#pragma once

#include <mc/deps/core/mce/Color.h>
#include <mc/math/Vec2.h>
#include <mc/math/Vec3.h>
#include <mc/world/AutomaticID.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/MolangVariableMap.h>
#include <mc/world/level/levelgen/structure/BoundingBox.h>
#include <mc/world/phys/AABB.h>
#include <memory>

namespace bsci {
class ParticleSender {
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    ParticleSender(
        float                        lifeTime      = 1.0f,
        float                        lineThickness = 0.09f,
        std::optional<ActorUniqueID> actorId       = std::nullopt
    );
    ~ParticleSender();

    void particle(
        std::string const& name,
        Vec3 const&        pos,
        DimensionType      dim,
        MolangVariableMap  var = {}
    ) const;

    void line(
        DimensionType     dim,
        Vec3 const&       begin,
        Vec3 const&       end,
        mce::Color const& color = mce::Color::WHITE
    ) const;

    void box(DimensionType dim, AABB const& box, mce::Color const& color = mce::Color::WHITE) const;
};
} // namespace bsci