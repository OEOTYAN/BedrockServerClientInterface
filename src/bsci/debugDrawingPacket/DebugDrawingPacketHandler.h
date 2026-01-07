#pragma once

#include "bsci/GeometryGroup.h"

namespace bsci {
class DebugDrawingPacketHandler : public GeometryGroup {
private:
    class Impl;

public:
    DebugDrawingPacketHandler();
    ~DebugDrawingPacketHandler();

private:
    void tick(Tick const& tick) override;

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

    GeoId
    box(DimensionType        dim,
        AABB const&          box,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}) override;

    GeoId circle2(
        DimensionType     dim,
        Vec3 const&       center,
        Vec3 const&       normal,
        float             radius,
        mce::Color const& color = mce::Color::WHITE()
    ) override;

    // GeoId cylinder(
    //     DimensionType        dim,
    //     Vec3 const&          topCenter,
    //     Vec3 const&          bottomCenter,
    //     float                radius,
    //     mce::Color const&    color     = mce::Color::WHITE(),
    //     std::optional<float> thickness = {}
    // ) override;

    GeoId sphere2(
        DimensionType        dim,
        Vec3 const&          center,
        float                radius,
        mce::Color const&    color        = mce::Color::WHITE(),
        std::optional<uchar> mNumSegments = {}
    ) override;

    GeoId arrow(
        DimensionType        dim,
        Vec3 const&          begin,
        Vec3 const&          end,
        mce::Color const&    color            = mce::Color::WHITE(),
        std::optional<float> mArrowHeadLength = {},
        std::optional<float> mArrowHeadRadius = {},
        std::optional<uchar> mNumSegments     = {}
    ) override;

    GeoId text(
        DimensionType        dim,
        Vec3 const&          pos,
        std::string&         text,
        mce::Color const&    color = mce::Color::WHITE(),
        std::optional<float> scale = {}
    ) override;

    bool remove(GeoId) override;

    GeoId merge(std::span<GeoId>) override;

    bool shift(GeoId, Vec3 const&) override;
};
} // namespace bsci