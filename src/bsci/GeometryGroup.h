#pragma once

#include "bsci/Marcos.h"
#include "bsci/utils/Math.h"
#include <mc/deps/core/mce/Color.h>
#include <mc/world/AutomaticID.h>

namespace bsci {
class GeometryGroup {
public:
    struct GeoId {
        uint64             value;
        constexpr bool     operator==(GeoId const& other) const { return other.value == value; }
        explicit constexpr operator bool() const { return value == 0; }
    };

protected:
    GeoId getNextGeoId() const;

public:
    BSCI_API static std::unique_ptr<GeometryGroup> createDefault();

    virtual ~GeometryGroup() = default;

    virtual GeoId point(
        DimensionType        dim,
        Vec3 const&          pos,
        mce::Color const&    color  = mce::Color::WHITE,
        std::optional<float> radius = {}
    ) = 0;

    virtual GeoId line(
        DimensionType        dim,
        Vec3 const&          begin,
        Vec3 const&          end,
        mce::Color const&    color     = mce::Color::WHITE,
        std::optional<float> thickness = {}
    ) = 0;

    virtual bool remove(GeoId) = 0;

    virtual GeoId merge(std::span<GeoId>) = 0;

    virtual bool shift(GeoId, Vec3 const&) = 0;

    virtual GeoId line(
        DimensionType        dim,
        std::span<Vec3>      dots,
        mce::Color const&    color     = mce::Color::WHITE,
        std::optional<float> thickness = {}
    );

    virtual GeoId
    box(DimensionType        dim,
        AABB const&          box,
        mce::Color const&    color     = mce::Color::WHITE,
        std::optional<float> thickness = {});

    virtual GeoId circle(
        DimensionType        dim,
        Vec3 const&          center,
        Vec3 const&          normal,
        float                radius,
        mce::Color const&    color     = mce::Color::WHITE,
        std::optional<float> thickness = {}
    );

    virtual GeoId sphere(
        DimensionType        dim,
        Vec3 const&          center,
        float                radius,
        mce::Color const&    color     = mce::Color::WHITE,
        std::optional<float> thickness = {}
    );
};
} // namespace bsci

namespace std {
template <>
struct hash<bsci::GeometryGroup::GeoId> {
    size_t operator()(bsci::GeometryGroup::GeoId const& id) {
        return std::hash<uint64>{}(id.value);
    }
};
} // namespace std