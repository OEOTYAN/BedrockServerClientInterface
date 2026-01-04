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
    ph_flat_hash_map<GeoId, std::pair<std::unique_ptr<DebugDrawerPacket>, bool>>
                                     geoPackets; // bool = true 表示已启用ParticleSpawner
    std::unique_ptr<ParticleSpawner> particleSpawner = std::make_unique<ParticleSpawner>();

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

    bool remove(GeoId) override;

    GeoId merge(std::span<GeoId>) override;
};
} // namespace bsci