#pragma once

#include "bsci/GeometryGroup.h"
#include "bsci/particle/ParticleSpawner.h"
#include "mc/network/packet/DebugDrawerPacket.h"

#include <parallel_hashmap/phmap.h>


template <class K, class V, size_t N = 4, class M = std::shared_mutex>
using ph_flat_hash_map = phmap::parallel_flat_hash_map<
    K,
    V,
    phmap::priv::hash_default_hash<K>,
    phmap::priv::hash_default_eq<K>,
    phmap::priv::Allocator<phmap::priv::Pair<const K, V>>,
    N,
    M>;

namespace bsci {
class DebugDrawingPacketHandler : public GeometryGroup {
private:
    std::unique_ptr<ParticleSpawner> particleSpawner =
        std::make_unique<ParticleSpawner>(); // 向前兼容
    ph_flat_hash_map<GeoId, std::pair<std::vector<std::unique_ptr<DebugDrawerPacket>>, bool>>
        geoPackets; // bool = true 表示已启用ParticleSpawner

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

    GeoId
    box(DimensionType        dim,
        AABB const&          box,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}) override;

    BSCI_API virtual GeoId circle2(
        DimensionType     dim,
        Vec3 const&       center,
        Vec3 const&       normal,
        float             radius,
        mce::Color const& color = mce::Color::WHITE()
    );

    // GeoId cylinder(
    //     DimensionType        dim,
    //     Vec3 const&          topCenter,
    //     Vec3 const&          bottomCenter,
    //     float                radius,
    //     mce::Color const&    color     = mce::Color::WHITE(),
    //     std::optional<float> thickness = {}
    // ) override;

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
        std::string          text,
        mce::Color const&    color = mce::Color::WHITE(),
        std::optional<float> scale = {}
    );

    bool remove(GeoId) override;

    GeoId merge(std::span<GeoId>) override;

    bool shift(GeoId, Vec3 const&) override;
};
} // namespace bsci