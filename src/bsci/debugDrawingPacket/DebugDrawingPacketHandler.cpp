#include "DebugDrawingPacketHandler.h"
#include "BedrockServerClientInterface.h"
#include "bsci/particle/ParticleSpawner.h"

#include "mc/network/packet/DebugDrawerPacket.h"
#include "mc/network/packet/DebugDrawerPacketPayload.h"
#include "mc/network/packet/ShapeDataPayload.h"
#include "mc/network/packet/SphereDataPayload.h"
#include "mc/network/packet/TextDataPayload.h"
#include <cstddef>
#include <ll/api/memory/Hook.h>
#include <ll/api/memory/Memory.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/service/GamingStatus.h>
#include <ll/api/thread/ServerThreadExecutor.h>
#include <mc/util/Timer.h>
#include <mc/world/Minecraft.h>
#include <memory>
#include <optional>
#include <utility>

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

TextDataPayload::TextDataPayload() = default;
TextDataPayload::TextDataPayload(TextDataPayload const& cp) { mText = cp.mText; };
ShapeDataPayload::ShapeDataPayload()                                                = default;
DebugDrawerPacketPayload::DebugDrawerPacketPayload()                                = default;
DebugDrawerPacketPayload::DebugDrawerPacketPayload(DebugDrawerPacketPayload const&) = default;

namespace bsci {
std::unique_ptr<GeometryGroup> GeometryGroup::createDefault() {
    return std::make_unique<DebugDrawingPacketHandler>();
}

static std::atomic<uint64_t> nextId_{UINT64_MAX};

class DebugDrawingPacketHandler::Impl : public GeometryGroup::ImplBase {
public:
    std::unique_ptr<ParticleSpawner> particleSpawner =
        std::make_unique<ParticleSpawner>(); // 向前兼容
    ph_flat_hash_map<GeoId, std::unique_ptr<DebugDrawerPacket>, 6>
        geoPackets; // ptr = nullptr 表示使用ParticleSpawner
    ph_flat_hash_map<GeoId, std::pair<std::vector<GeoId>, bool>>
        geoGroup; // bool = true 表示已启用ParticleSpawner

public:
    void sendPacketImmediately(DebugDrawerPacket& pkt) {

        if (BedrockServerClientInterface::getInstance().getConfig().particle.delayUndate) {
            return;
        }
        if (pkt.mShapes->empty()) return;
        ll::thread::ServerThreadExecutor::getDefault().execute([pkt] {
            pkt.sendTo((*pkt.mShapes)[0].mLocation->value(), (*pkt.mShapes)[0].mDimensionId);
        });
    }
};

DebugDrawingPacketHandler::DebugDrawingPacketHandler() {
    impl = std::make_unique<Impl>();
    this->addToTickList();
}

DebugDrawingPacketHandler::~DebugDrawingPacketHandler() = default;

void DebugDrawingPacketHandler::tick(Tick const& tick) {
    auto const tablePerTick =
        BedrockServerClientInterface::getInstance().getConfig().particle.tablePerTick;
    auto begin = (tick.tickID % (64 / tablePerTick)) * tablePerTick;
    for (size_t i = 0; i < tablePerTick; i++) {
        getImpl<Impl>().geoPackets.with_submap(begin + i, [&](auto& map) {
            for (auto& [id, pkt] : map) {
                if (pkt)
                    pkt->sendTo(
                        (*pkt->mShapes)[0].mLocation->value(),
                        (*pkt->mShapes)[0].mDimensionId
                    ); // tick must in server thread
            }
        });
    }
}

GeometryGroup::GeoId DebugDrawingPacketHandler::point(
    DimensionType        dim,
    Vec3 const&          pos,
    mce::Color const&    color,
    std::optional<float> radius
) {
    auto geoId = getImpl<Impl>().particleSpawner->point(dim, pos, color, radius);
    getImpl<Impl>().geoPackets.emplace(geoId, nullptr);
    return geoId;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::line(
    DimensionType        dim,
    Vec3 const&          begin,
    Vec3 const&          end,
    mce::Color const&    color,
    std::optional<float> thickness
) {
    if (begin == end) return {0};
    if (thickness.has_value()) {
        auto geoId = getImpl<Impl>().particleSpawner->line(dim, begin, end, color, thickness);
        getImpl<Impl>().geoPackets.emplace(geoId, nullptr);
        return geoId;
    }
    auto packet = std::make_unique<DebugDrawerPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    ShapeDataPayload shape;
    shape.mNetworkId        = nextId_.fetch_sub(1);
    shape.mShapeType        = ScriptModuleDebugUtilities::ScriptDebugShapeType::Line;
    shape.mLocation         = begin;
    shape.mColor            = color;
    shape.mDimensionId      = dim;
    shape.mExtraDataPayload = LineDataPayload{.mEndLocation = end};
    packet->mShapes->emplace_back(std::move(shape));
    packet->sendTo(begin, dim);
    auto id = GeometryGroup::getNextGeoId();
    getImpl<Impl>().geoPackets.emplace(id, std::move(packet));
    return id;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::box(
    DimensionType        dim,
    AABB const&          box,
    mce::Color const&    color,
    std::optional<float> thickness
) {
    if (thickness.has_value()) {
        auto geoId = getImpl<Impl>().particleSpawner->box(dim, box, color, thickness);
        getImpl<Impl>().geoPackets.emplace(geoId, nullptr);
        return geoId;
    }
    auto packet = std::make_unique<DebugDrawerPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    ShapeDataPayload shape;
    shape.mNetworkId        = nextId_.fetch_sub(1);
    shape.mShapeType        = ScriptModuleDebugUtilities::ScriptDebugShapeType::Box;
    shape.mLocation         = box.min;
    shape.mColor            = color;
    shape.mDimensionId      = dim;
    shape.mExtraDataPayload = BoxDataPayload{.mBoxBound = box.max - box.min};
    packet->mShapes->emplace_back(std::move(shape));
    packet->sendTo(box.min, dim);
    auto id = GeometryGroup::getNextGeoId();
    getImpl<Impl>().geoPackets.emplace(id, std::move(packet));
    return id;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::circle2(
    DimensionType     dim,
    Vec3 const&       center,
    Vec3 const&       normal,
    float             radius,
    mce::Color const& color
) {
    auto packet = std::make_unique<DebugDrawerPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    ShapeDataPayload shape;
    shape.mNetworkId   = nextId_.fetch_sub(1);
    shape.mShapeType   = ScriptModuleDebugUtilities::ScriptDebugShapeType::Circle;
    shape.mRotation    = normal; // 待测试
    shape.mLocation    = center;
    shape.mScale       = radius; // 待测试
    shape.mColor       = color;
    shape.mDimensionId = dim;
    packet->mShapes->emplace_back(std::move(shape));
    packet->sendTo(center, dim);
    auto id = GeometryGroup::getNextGeoId();
    getImpl<Impl>().geoPackets.emplace(id, std::move(packet));
    return id;
}

// GeometryGroup::GeoId DebugDrawingPacketHandler::cylinder(
//     DimensionType        dim,
//     Vec3 const&          topCenter,
//     Vec3 const&          bottomCenter,
//     float                radius,
//     mce::Color const&    color,
//     std::optional<float> thickness
// ) {
//     if (thickness.has_value()) {
//         auto geoId =
//             this->particleSpawner->cylinder(dim, topCenter, bottomCenter, radius, color,
//             thickness);
//         this->geoPackets.emplace(geoId, std::make_pair(nullptr, true));
//         return geoId;
//     }

//     auto const& config = BedrockServerClientInterface::getInstance().getConfig().particle;

//     size_t const points = std::clamp(
//         (size_t)std::ceil(radius * std::numbers::pi * 2 / config.minCircleSpacing),
//         7ui64,
//         config.maxCircleSegments

//     );
//     auto const [t, b] = branchlessONB((topCenter - bottomCenter).normalize());
//     auto const delta  = std::numbers::pi * 2 / (double)points;

//     std::vector<GeoId> ids;
//     ids.reserve(points + 2);
//     ids.emplace_back(this->circle(dim, topCenter, Vec3(0, 1, 0), radius, color));
//     ids.emplace_back(this->circle(dim, bottomCenter, Vec3(0, 1, 0), radius, color));
//     for (size_t i{1}; i <= points; i++) {
//         double theta = (double)i * delta;
//         Vec3   pos   = t * (radius * std::cos(theta)) + b * radius * std::sin(theta);
//         ids.emplace_back(this->line(dim, topCenter + pos, bottomCenter + pos, color,
//         thickness));
//     }
//     return merge(ids);
// }

GeometryGroup::GeoId DebugDrawingPacketHandler::sphere2(
    DimensionType        dim,
    Vec3 const&          center,
    float                radius,
    mce::Color const&    color,
    std::optional<uchar> mNumSegments
) {
    auto packet = std::make_unique<DebugDrawerPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    ShapeDataPayload shape;
    shape.mNetworkId   = nextId_.fetch_sub(1);
    shape.mShapeType   = ScriptModuleDebugUtilities::ScriptDebugShapeType::Sphere;
    shape.mLocation    = center;
    shape.mScale       = radius; // 待测试
    shape.mColor       = color;
    shape.mDimensionId = dim;
    if (mNumSegments.has_value()) {
        shape.mExtraDataPayload = SphereDataPayload{.mNumSegments = mNumSegments.value()};
    }
    packet->mShapes->emplace_back(std::move(shape));
    packet->sendTo(center, dim);
    auto id = GeometryGroup::getNextGeoId();
    getImpl<Impl>().geoPackets.emplace(id, std::move(packet));
    return id;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::arrow(
    DimensionType        dim,
    Vec3 const&          begin,
    Vec3 const&          end,
    mce::Color const&    color,
    std::optional<float> mArrowHeadLength,
    std::optional<float> mArrowHeadRadius,
    std::optional<uchar> mNumSegments
) {
    if (begin == end) return {0};
    auto packet = std::make_unique<DebugDrawerPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    ShapeDataPayload shape;
    shape.mNetworkId        = nextId_.fetch_sub(1);
    shape.mShapeType        = ScriptModuleDebugUtilities::ScriptDebugShapeType::Arrow;
    shape.mLocation         = begin;
    shape.mColor            = color;
    shape.mDimensionId      = dim;
    shape.mExtraDataPayload = ArrowDataPayload{
        .mEndLocation     = end,
        .mArrowHeadLength = mArrowHeadLength,
        .mArrowHeadRadius = mArrowHeadRadius,
        .mNumSegments     = mNumSegments
    };
    packet->mShapes->emplace_back(std::move(shape));
    packet->sendTo(begin, dim);
    auto id = GeometryGroup::getNextGeoId();
    getImpl<Impl>().geoPackets.emplace(id, std::move(packet));
    return id;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::text(
    DimensionType        dim,
    Vec3 const&          pos,
    std::string&         text,
    mce::Color const&    color,
    std::optional<float> scale
) {
    auto packet = std::make_unique<DebugDrawerPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    ShapeDataPayload shape;
    shape.mNetworkId   = nextId_.fetch_sub(1);
    shape.mShapeType   = ScriptModuleDebugUtilities::ScriptDebugShapeType::Text;
    shape.mLocation    = pos;
    shape.mScale       = scale;
    shape.mColor       = color;
    shape.mDimensionId = dim;
    TextDataPayload extraDataPayload;
    extraDataPayload.mText  = std::move(text);
    shape.mExtraDataPayload = std::move(extraDataPayload);
    packet->mShapes->emplace_back(std::move(shape));
    packet->sendTo(pos, dim);
    auto id = GeometryGroup::getNextGeoId();
    getImpl<Impl>().geoPackets.emplace(id, std::move(packet));
    return id;
}

bool DebugDrawingPacketHandler::remove(GeoId id) {
    if (id.value == 0) {
        return false;
    }
    if (!getImpl<Impl>().geoGroup.erase_if(id, [this](auto&& iter) {
            if (iter.second.second) getImpl<Impl>().particleSpawner->remove(iter.first);
            for (auto& subId : iter.second.first) {
                getImpl<Impl>().geoPackets.erase_if(subId, [this](auto&& it) {
                    if (it.second != nullptr) {
                        for (auto& shape : *it.second->mShapes) shape.mShapeType = std::nullopt;
                        getImpl<Impl>().sendPacketImmediately(*it.second);
                    } else {
                        getImpl<Impl>().particleSpawner->remove(it.first);
                    }
                    return true;
                });
            }
            return true;
        })) {
        return getImpl<Impl>().geoPackets.erase_if(id, [this](auto&& iter) {
            if (iter.second != nullptr) {
                for (auto& shape : *iter.second->mShapes) shape.mShapeType = std::nullopt;
                getImpl<Impl>().sendPacketImmediately(*iter.second);
            } else getImpl<Impl>().particleSpawner->remove(iter.first);
            return true;
        });
    }
    return true;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::merge(std::span<GeoId> ids) {
    if (ids.empty()) {
        return {0};
    }
    std::vector<GeoId> res;
    std::vector<GeoId> particleIds;
    res.reserve(ids.size());
    for (auto const& sid : ids) {
        if (!getImpl<Impl>().geoGroup.erase_if(sid, [&res, &particleIds](auto&& iter) {
                if (iter.second.second) particleIds.emplace_back(iter.first);
                res.append_range(std::move(iter.second.first));
                return true;
            })) {
            getImpl<Impl>().geoPackets.modify_if(sid, [&res, &particleIds](auto&& iter) {
                if (iter.second == nullptr) particleIds.emplace_back(iter.first);
                res.push_back(iter.first);
                return true;
            });
        }
    }
    auto newId              = getImpl<Impl>().particleSpawner->merge(particleIds);
    bool useParticleSpawner = newId.value != 0;
    if (!useParticleSpawner) {
        if (res.empty()) return {0};
        else newId = GeometryGroup::getNextGeoId();
    }
    getImpl<Impl>().geoGroup.try_emplace(newId, std::make_pair(std::move(res), useParticleSpawner));
    return newId;
}

bool DebugDrawingPacketHandler::shift(GeoId id, Vec3 const& v) {
    if (id.value == 0) {
        return false;
    }
    if (!getImpl<Impl>().geoGroup.modify_if(id, [this, &v](auto&& i) {
            for (auto& subId : i.second.first) {
                getImpl<Impl>().geoPackets.modify_if(subId, [this, &v](auto&& iter) {
                    if (iter.second == nullptr) {
                        getImpl<Impl>().particleSpawner->shift(iter.first, v);
                        return;
                    } else {
                        for (auto& shape : *iter.second->mShapes) {
                            shape.mLocation->value() += v;
                            if (std::holds_alternative<ArrowDataPayload>(*shape.mExtraDataPayload
                                )) {
                                std::get<ArrowDataPayload>(*shape.mExtraDataPayload)
                                    .mEndLocation->value() += v;
                            } else if (std::holds_alternative<LineDataPayload>(
                                           *shape.mExtraDataPayload
                                       )) {
                                *std::get<LineDataPayload>(*shape.mExtraDataPayload).mEndLocation +=
                                    v;
                            }
                        }
                        getImpl<Impl>().sendPacketImmediately(*iter.second);
                    }
                });
            }
        })) {
        getImpl<Impl>().geoPackets.modify_if(id, [this, &v](auto&& iter) {
            if (iter.second == nullptr) {
                getImpl<Impl>().particleSpawner->shift(iter.first, v);
                return;
            } else {
                for (auto& shape : *iter.second->mShapes) {
                    shape.mLocation->value() += v;
                    if (std::holds_alternative<ArrowDataPayload>(*shape.mExtraDataPayload)) {
                        std::get<ArrowDataPayload>(*shape.mExtraDataPayload)
                            .mEndLocation->value() += v;
                    } else if (std::holds_alternative<LineDataPayload>(*shape.mExtraDataPayload)) {
                        *std::get<LineDataPayload>(*shape.mExtraDataPayload).mEndLocation += v;
                    }
                }
                getImpl<Impl>().sendPacketImmediately(*iter.second);
            }
        });
    }
    return true;
}

} // namespace bsci