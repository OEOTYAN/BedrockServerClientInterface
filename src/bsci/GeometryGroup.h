#pragma once

#include "bsci/Marcos.h"
#include "mc/world/phys/AABB.h"

#include <mc/deps/core/math/Color.h>
#include <mc/deps/core/utility/AutomaticID.h>

namespace bsci {
class GeometryGroup {
public:
    struct GeoId {
        uint64             value;
        constexpr bool     operator==(GeoId const& other) const { return other.value == value; }
        explicit constexpr operator bool() const { return value == 0; }
    };

protected:
    BSCI_API GeoId getNextGeoId() const;

public:
    BSCI_API static std::unique_ptr<GeometryGroup> createDefault();

    BSCI_API virtual ~GeometryGroup() = default;

    virtual GeoId point(
        DimensionType        dim,
        Vec3 const&          pos,
        mce::Color const&    color  = mce::Color::WHITE(),
        std::optional<float> radius = {}
    ) = 0;

    virtual GeoId line(
        DimensionType        dim,
        Vec3 const&          begin,
        Vec3 const&          end,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    ) = 0;

    virtual bool remove(GeoId) = 0;

    virtual GeoId merge(std::span<GeoId>) = 0;

    virtual bool shift(GeoId, Vec3 const&) = 0;

    BSCI_API virtual GeoId line(
        DimensionType        dim,
        std::span<Vec3>      dots,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    );

    BSCI_API virtual GeoId
    box(DimensionType        dim,
        AABB const&          box,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {});

    BSCI_API virtual GeoId circle(
        DimensionType        dim,
        Vec3 const&          center,
        Vec3 const&          normal,
        float                radius,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    );

    BSCI_API virtual GeoId cylinder(
        DimensionType        dim,
        Vec3 const&          topCenter,
        Vec3 const&          bottomCenter,
        float                radius,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    );

    BSCI_API virtual GeoId sphere(
        DimensionType        dim,
        Vec3 const&          center,
        float                radius,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    );

    BSCI_API virtual GeoId circle2(
        DimensionType     dim,
        Vec3 const&       center,
        Vec3 const&       normal,
        float             radius,
        mce::Color const& color = mce::Color::WHITE()
    );

    BSCI_API virtual GeoId sphere2(
        DimensionType        dim,
        Vec3 const&          center,
        float                radius,
        mce::Color const&    color        = mce::Color::WHITE(),
        std::optional<uchar> mNumSegments = {}
    );

    BSCI_API virtual GeoId arrow(
        DimensionType        dim,
        Vec3 const&          begin,
        Vec3 const&          end,
        mce::Color const&    color            = mce::Color::WHITE(),
        std::optional<float> mArrowHeadLength = {},
        std::optional<float> mArrowHeadRadius = {},
        std::optional<uchar> mNumSegments     = {}
    );

    BSCI_API virtual GeoId text(
        DimensionType        dim,
        Vec3 const&          pos,
        std::string&         text,
        mce::Color const&    color = mce::Color::WHITE(),
        std::optional<float> scale = {}
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
