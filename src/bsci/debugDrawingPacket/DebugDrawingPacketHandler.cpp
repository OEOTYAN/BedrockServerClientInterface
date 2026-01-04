#include "DebugDrawingPacketHandler.h"

// #include "mc/network/packet/DebugDrawerPacketPayload.h"
// #include "mc/network/ServerNetworkHandler.h"
#include "mc/network/packet/DebugDrawerPacket.h"
#include "mc/network/packet/DebugDrawerPacketPayload.h"
#include "mc/network/packet/ShapeDataPayload.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>


// ShapeDataPayload::ShapeDataPayload()                 = default;
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
    this->geoPackets.emplace(geoId, std::make_pair(nullptr, true));
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
        this->geoPackets.emplace(geoId, std::make_pair(nullptr, true));
        return geoId;
    }
    auto packet = std::make_unique<DebugDrawerPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    ShapeDataPayload shape;
    shape.mNetworkId        = nextId_.fetch_sub(1);
    shape.mShapeType        = ScriptModuleDebugUtilities::ScriptDebugShapeType::Line;
    shape.mLocation         = begin;
    shape.mDimensionId      = dim;
    shape.mExtraDataPayload = LineDataPayload(end);
    packet->mShapes->emplace_back(std::move(shape));
    packet->sendTo(begin, dim);
    auto id = GeometryGroup::getNextGeoId();
    this->geoPackets.emplace(id, std::make_pair(std::move(packet), false));
    return id;
}

bool DebugDrawingPacketHandler::remove(GeoId id) {
    if (id.value == 0) {
        return false;
    }
    this->geoPackets.erase_if(id, [this, &id](auto&& iter) {
        if (iter.second.second) this->particleSpawner->remove(id);
        for (auto& shape : iter.second.first->mShapes.value()) shape.mShapeType = std::nullopt;
        iter.second.first->sendToClients();
        return true;
    });
    return true;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::merge(std::span<GeoId> ids) {
    if (ids.empty()) {
        return {};
    }
    std::vector<GeoId>                              particleIds;
    std::vector<std::unique_ptr<DebugDrawerPacket>> packets;
    packets.reserve(ids.size());
    for (auto const& id : ids) {
        this->geoPackets.erase_if(id, [&particleIds, &packets](auto&& iter) {
            if (iter.second.first) packets.push_back(std::move(iter.second.first));
            if (iter.second.second) particleIds.push_back(iter.first);
            return true;
        });
    }
    std::unique_ptr<DebugDrawerPacket> newPacket = nullptr;
    std::optional<GeoId>               newId;
    if (!packets.empty()) newPacket = std::make_unique<DebugDrawerPacket>();
    if (!particleIds.empty()) newId = this->particleSpawner->merge(particleIds);
    if (!newId.has_value()) {
        if (!newPacket) return {};
        else newId = GeometryGroup::getNextGeoId();
    }
    if (newPacket) {
        size_t totalShapes = 0;
        for (const auto& pkt : packets) {
            totalShapes += pkt->mShapes->size();
        }
        newPacket->mShapes->reserve(totalShapes);
        for (auto& pkt : packets) {
            if (!pkt->mShapes->empty()) {
                newPacket->mShapes->insert(
                    newPacket->mShapes->end(),
                    std::make_move_iterator(pkt->mShapes->begin()),
                    std::make_move_iterator(pkt->mShapes->end())
                );
            }
        }
    }
    this->geoPackets.emplace(
        newId.value(),
        std::make_pair(std::move(newPacket), !particleIds.empty())
    );

    return newId.value();
}
} // namespace bsci