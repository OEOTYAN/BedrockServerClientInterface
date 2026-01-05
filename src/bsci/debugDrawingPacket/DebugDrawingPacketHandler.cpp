#include "DebugDrawingPacketHandler.h"
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
#include <mc/util/Timer.h>
#include <mc/world/Minecraft.h>
#include <memory>
#include <optional>
#include <utility>


TextDataPayload::TextDataPayload() = default;
TextDataPayload::TextDataPayload(TextDataPayload const& cp) { mText = cp.mText; };
ShapeDataPayload::ShapeDataPayload()                 = default;
DebugDrawerPacketPayload::DebugDrawerPacketPayload() = default;

namespace bsci {
static std::atomic<uint64_t> nextId_{UINT64_MAX};

GeometryGroup::GeoId DebugDrawingPacketHandler::point(
    DimensionType        dim,
    Vec3 const&          pos,
    mce::Color const&    color,
    std::optional<float> radius
) {
    auto geoId = this->particleSpawner->point(dim, pos, color, radius);
    this->geoPackets.emplace(
        geoId,
        std::make_pair(std::vector<std::unique_ptr<DebugDrawerPacket>>(), true)
    );
    return geoId;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::line(
    DimensionType        dim,
    Vec3 const&          begin,
    Vec3 const&          end,
    mce::Color const&    color,
    std::optional<float> thickness
) {
    if (begin == end) return {};
    if (thickness.has_value()) {
        auto geoId = this->particleSpawner->line(dim, begin, end, color, thickness);
        this->geoPackets.emplace(
            geoId,
            std::make_pair(std::vector<std::unique_ptr<DebugDrawerPacket>>(), true)
        );
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
    packet->sendTo((begin + end) / 2, dim);
    auto                                            id = GeometryGroup::getNextGeoId();
    std::vector<std::unique_ptr<DebugDrawerPacket>> packets;
    packets.push_back(std::move(packet));
    this->geoPackets.emplace(id, std::make_pair(std::move(packets), false));
    return id;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::box(
    DimensionType        dim,
    AABB const&          box,
    mce::Color const&    color,
    std::optional<float> thickness
) {
    if (thickness.has_value()) {
        auto geoId = this->particleSpawner->box(dim, box, color, thickness);
        this->geoPackets.emplace(
            geoId,
            std::make_pair(std::vector<std::unique_ptr<DebugDrawerPacket>>(), true)
        );
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
    packet->sendTo(box.center(), dim);
    auto                                            id = GeometryGroup::getNextGeoId();
    std::vector<std::unique_ptr<DebugDrawerPacket>> packets;
    packets.push_back(std::move(packet));
    this->geoPackets.emplace(id, std::make_pair(std::move(packets), false));
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
    auto                                            id = GeometryGroup::getNextGeoId();
    std::vector<std::unique_ptr<DebugDrawerPacket>> packets;
    packets.push_back(std::move(packet));
    this->geoPackets.emplace(id, std::make_pair(std::move(packets), false));
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
//         ids.emplace_back(this->line(dim, topCenter + pos, bottomCenter + pos, color, thickness));
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
    auto                                            id = GeometryGroup::getNextGeoId();
    std::vector<std::unique_ptr<DebugDrawerPacket>> packets;
    packets.push_back(std::move(packet));
    this->geoPackets.emplace(id, std::make_pair(std::move(packets), false));
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
    if (begin == end) return {};
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
    packet->sendTo((begin + end) / 2, dim);
    auto                                            id = GeometryGroup::getNextGeoId();
    std::vector<std::unique_ptr<DebugDrawerPacket>> packets;
    packets.push_back(std::move(packet));
    this->geoPackets.emplace(id, std::make_pair(std::move(packets), false));
    return id;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::text(
    DimensionType        dim,
    Vec3 const&          pos,
    std::string          text,
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
    auto                                            id = GeometryGroup::getNextGeoId();
    std::vector<std::unique_ptr<DebugDrawerPacket>> packets;
    packets.push_back(std::move(packet));
    this->geoPackets.emplace(id, std::make_pair(std::move(packets), false));
    return id;
}

bool DebugDrawingPacketHandler::remove(GeoId id) {
    if (id.value == 0) {
        return false;
    }
    this->geoPackets.erase_if(id, [this, &id](auto&& iter) {
        if (iter.second.second) this->particleSpawner->remove(id);
        for (const auto& packet : iter.second.first) {
            for (auto& shape : *packet->mShapes) shape.mShapeType = std::nullopt;
            packet->sendToClients();
        }
        return true;
    });
    return true;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::merge(std::span<GeoId> ids) {
    if (ids.empty()) {
        return {};
    }
    std::vector<GeoId>                                           particleIds;
    std::vector<std::vector<std::unique_ptr<DebugDrawerPacket>>> packetses;
    packetses.reserve(ids.size());
    for (auto const& id : ids) {
        this->geoPackets.erase_if(id, [&particleIds, &packetses](auto&& iter) {
            if (!iter.second.first.empty()) packetses.push_back(std::move(iter.second.first));
            if (iter.second.second) particleIds.push_back(iter.first);
            return true;
        });
    }
    std::vector<std::unique_ptr<DebugDrawerPacket>> newPackets;
    auto newId = this->particleSpawner->merge(particleIds);
    // if (!packetses.empty()) newPacket = std::make_unique<DebugDrawerPacket>();
    if (newId.value == 0) {
        if (packetses.empty()) return {};
        else newId = GeometryGroup::getNextGeoId();
    }
    // if (newPacket) {
    size_t totalPackets = 0;
    for (const auto& pkts : packetses) {
        totalPackets += pkts.size();
    }
    newPackets.reserve(totalPackets);
    for (auto& pkts : packetses) {
        newPackets.insert(
            newPackets.end(),
            std::make_move_iterator(pkts.begin()),
            std::make_move_iterator(pkts.end())
        );
    }
    this->geoPackets.emplace(newId, std::make_pair(std::move(newPackets), !particleIds.empty()));

    return newId;
}

bool DebugDrawingPacketHandler::shift(GeoId id, Vec3 const& v) {
    if (id.value == 0) {
        return false;
    }
    this->geoPackets.modify_if(id, [this, &v](auto&& iter) {
        if (iter.second.second) this->particleSpawner->shift(iter.first, v);
        for (const auto& pkt : iter.second.first) {
            for (auto& shape : *pkt->mShapes) {
                shape.mLocation->value() += v;
                if (std::holds_alternative<ArrowDataPayload>(*shape.mExtraDataPayload)) {
                    std::get<ArrowDataPayload>(*shape.mExtraDataPayload).mEndLocation->value() += v;
                } else if (std::holds_alternative<LineDataPayload>(*shape.mExtraDataPayload)) {
                    *std::get<LineDataPayload>(*shape.mExtraDataPayload).mEndLocation += v;
                }
            }
            if (!pkt->mShapes->empty())
                pkt->sendTo((*pkt->mShapes)[0].mLocation->value(), (*pkt->mShapes)[0].mDimensionId);
        }
        return true;
    });
    return true;
}

LL_TYPE_INSTANCE_HOOK(
    DebugDrawingPacketHandlerHook,
    HookPriority::Normal,
    Minecraft,
    &Minecraft::update,
    bool
) {
    if (mSimTimer.mTicks && ll::getGamingStatus() == ll::GamingStatus::Running) {}
    return origin();
}

} // namespace bsci