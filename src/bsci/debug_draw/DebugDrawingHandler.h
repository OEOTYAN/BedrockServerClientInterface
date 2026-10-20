#pragma once

#include "bsci/PacketGeoGroup.h"

namespace bsci {
class DebugDrawingHandler : public PacketGeoGroup {
private:
    class State;
    std::unique_ptr<State> drawingState;

    using Base = PacketGeoGroup;

public:
    DebugDrawingHandler();
    ~DebugDrawingHandler();

public:
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

    GeoId circle(
        DimensionType        dim,
        Vec3 const&          center,
        Vec3 const&          normal,
        float                radius,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    ) override;

    GeoId sphere(
        DimensionType        dim,
        Vec3 const&          center,
        float                radius,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    ) override;

    GeoId arrow(
        DimensionType        dim,
        Vec3 const&          begin,
        Vec3 const&          end,
        mce::Color const&    color            = mce::Color::WHITE(),
        std::optional<float> mArrowHeadLength = {},
        std::optional<float> mArrowHeadRadius = {}
    ) override;

    GeoId text(
        DimensionType        dim,
        Vec3 const&          pos,
        std::string          text,
        mce::Color const&    color = mce::Color::WHITE(),
        std::optional<float> scale = {}
    ) override;

    bool remove(GeoId) override;

    GeoId merge(std::span<GeoId>) override;

    bool shift(GeoId, Vec3 const&) override;
};
} // namespace bsci
