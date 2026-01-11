#pragma once

#include "bsci/GeometryGroup.h"

namespace bsci {
class DebugDrawingPacketHandler : public GeometryGroup {
private:
    class Impl;
    std::unique_ptr<Impl> impl;

public:
    DebugDrawingPacketHandler();
    ~DebugDrawingPacketHandler();

public:
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
};
} // namespace bsci