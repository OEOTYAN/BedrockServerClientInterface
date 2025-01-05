#pragma once

#include "bsci/GeometryGroup.h"

#include <memory>

#include <mc/util/MolangVariableMap.h>
#include <mc/world/level/Tick.h>

namespace bsci {
class ParticleSpawner : public GeometryGroup {
    struct Impl;
    std::unique_ptr<Impl> impl;

    GeoId particle(
        DimensionType      dim,
        Vec3 const&        pos,
        std::string const& name,
        MolangVariableMap  var = {}
    ) const;

    void tick(Tick const& tick);

public:
    BSCI_API ParticleSpawner();

    ~ParticleSpawner() override;

    GeoId point(
        DimensionType        dim,
        Vec3 const&          pos,
        mce::Color const&    color  = mce::Color::WHITE(),
        std::optional<float> radius = {}
    ) override;

    GeoId line(
        DimensionType        dim,
        Vec3 const&          begin,
        Vec3 const&          end,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    ) override;

    bool remove(GeoId) override;

    GeoId merge(std::span<GeoId>) override;

    bool shift(GeoId, Vec3 const&) override;
};
} // namespace bsci
