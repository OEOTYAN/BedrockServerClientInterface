#pragma once

#include "bsci/Marcos.h"
#include "bsci/utils/Math.h"

#include <mc/deps/core/math/Color.h>
#include <mc/deps/core/utility/AutomaticID.h>
#include <mc/world/level/Tick.h>


namespace bsci {
class GeometryGroup {
public:
    struct GeoId {
        uint64             value = 0;
        constexpr bool     operator==(GeoId const& other) const { return other.value == value; }
        explicit constexpr operator bool() const { return value == 0; }
    };

protected:
    class ImplBase;

    std::unique_ptr<ImplBase> impl;

    template <typename T>
    T& getImplAs() {
        static_assert(std::is_base_of_v<ImplBase, T>, "T must inherit from ImplBase");
        return static_cast<T&>(*impl);
    }

protected:
    BSCI_API GeoId getNextGeoId() const;

public:
    BSCI_API static std::unique_ptr<GeometryGroup> createDefault();

    GeometryGroup();

    BSCI_API virtual ~GeometryGroup();

    virtual void tick(Tick const& tick) = 0;

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
