#include "DebugDrawingHandler.h"
#include "BedrockServerClientInterface.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <ll/api/base/Containers.h>
#include <ll/api/memory/Hook.h>

#include <mc/network/LoopbackPacketSender.h>
#include <mc/network/MinecraftPacketIds.h>
#include <mc/network/packet/LevelChunkPacket.h>
#include <mc/network/packet/PrimitiveShapeDataPayload.h>
#include <mc/network/packet/PrimitiveShapesPacket.h>
#include <mc/network/packet/PrimitiveShapesPacketPayload.h>
#include <mc/world/level/ChunkPos.h>

namespace bsci {
namespace {

using Packet         = PrimitiveShapesPacket;
using PacketPtr      = std::shared_ptr<Packet>;
using ChunkKey       = std::pair<ChunkPos, int>;
using GeoId          = GeometryGroup::GeoId;
using PrimitiveShape = PrimitiveShapeDataPayload;

constexpr size_t shapeDisplayRadius = 48;

PacketPtr makePacket(PrimitiveShape shape) {
    auto packet = std::make_shared<Packet>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    packet->mShapes->emplace_back(std::move(shape));
    return packet;
}

} // namespace

static std::atomic<uint64_t> nextId_{UINT64_MAX};

class DebugDrawingHandler::State {
public:
    using PacketPair = std::pair<GeoId, std::vector<std::weak_ptr<Packet>>>;

    struct PacketEntry {
        PacketPtr     packet;
        Vec3          position;
        DimensionType dimension;
    };

    struct GeoEntry {
        std::vector<PacketEntry> packets;
    };

    struct SendTarget {
        PacketPtr     packet;
        Vec3          position;
        DimensionType dimension;
    };

    struct Hook;

private:
    std::mutex                                                stateMutex;
    ll::ConcurrentDenseMap<GeoId, GeoEntry>                   geoPackets;
    ll::ConcurrentDenseMap<ChunkKey, std::vector<PacketPair>> chunkPackets;

    static ChunkKey makeChunkKey(Vec3 const& position, DimensionType dimension) {
        return {ChunkPos(position), static_cast<int>(dimension)};
    }

    static PacketPair* findPacketPair(std::vector<PacketPair>& entries, GeoId id) {
        auto it = std::find_if(entries.begin(), entries.end(), [id](PacketPair const& entry) {
            return entry.first == id;
        });
        return it == entries.end() ? nullptr : &*it;
    }

    void addChunkReferenceLocked(GeoId id, PacketEntry const& entry) {
        auto key              = makeChunkKey(entry.position, entry.dimension);
        auto [iter, inserted] = chunkPackets.try_emplace(key);
        auto& packetPairs     = iter->second;
        auto* packetPair      = findPacketPair(packetPairs, id);
        if (packetPair == nullptr) {
            packetPairs.emplace_back(id, std::vector<std::weak_ptr<Packet>>{entry.packet});
        } else {
            packetPair->second.emplace_back(entry.packet);
        }
        (void)inserted;
    }

    void removeChunkReferenceLocked(GeoId id, PacketEntry const& entry) {
        auto key = makeChunkKey(entry.position, entry.dimension);
        chunkPackets.erase_if(key, [&id, &entry](auto&& iter) {
            auto& packetPairs = iter.second;
            std::erase_if(packetPairs, [&id, &entry](PacketPair& packetPair) {
                if (packetPair.first != id) return false;

                std::erase_if(packetPair.second, [&entry](std::weak_ptr<Packet> const& weak) {
                    auto packet = weak.lock();
                    return !packet || packet.get() == entry.packet.get();
                });
                return packetPair.second.empty();
            });
            return packetPairs.empty();
        });
    }

    void addChunkReferencesLocked(GeoId id, GeoEntry const& entry) {
        for (auto const& packet : entry.packets) addChunkReferenceLocked(id, packet);
    }

    void removeChunkReferencesLocked(GeoId id, GeoEntry const& entry) {
        for (auto const& packet : entry.packets) removeChunkReferenceLocked(id, packet);
    }

    static void shiftPacket(Packet& packet, Vec3 const& offset) {
        for (auto& shape : *packet.mShapes) {
            if (!shape.mLocation->has_value()) continue;

            shape.mLocation->value() += offset;
            if (std::holds_alternative<ArrowDataPayload>(*shape.mExtraDataPayload)) {
                auto& endLocation =
                    std::get<ArrowDataPayload>(*shape.mExtraDataPayload).mEndLocation;
                if (endLocation->has_value()) endLocation->value() += offset;
            } else if (std::holds_alternative<LineDataPayload>(*shape.mExtraDataPayload)) {
                *std::get<LineDataPayload>(*shape.mExtraDataPayload).mEndLocation += offset;
            }
        }
    }

public:
    void addPacket(GeoId id, PacketPtr packet, DimensionType dimension, Vec3 const& position) {
        if (!packet) return;

        std::lock_guard lock{stateMutex};
        GeoEntry        entry;
        entry.packets.push_back(PacketEntry{std::move(packet), position, dimension});
        addChunkReferencesLocked(id, entry);
        geoPackets.emplace(id, std::move(entry));
    }

    std::vector<PacketPtr> packetsForChunk(ChunkKey const& key) {
        std::vector<PacketPtr> packets;
        std::lock_guard        lock{stateMutex};

        // A FullChunkData packet is sent to one recipient at a time. Keep this
        // index persistent so one recipient cannot consume another recipient's sync.
        chunkPackets.modify_if(key, [&packets](auto&& iter) {
            auto& packetPairs = iter.second;
            for (auto& packetPair : packetPairs) {
                std::erase_if(packetPair.second, [&packets](std::weak_ptr<Packet> const& weak) {
                    if (auto packet = weak.lock()) {
                        packets.emplace_back(std::move(packet));
                        return false;
                    }
                    return true;
                });
            }
            std::erase_if(packetPairs, [](PacketPair const& packetPair) {
                return packetPair.second.empty();
            });
            return packetPairs.empty();
        });
        return packets;
    }

    std::optional<GeoEntry> take(std::span<GeoId const> ids) {
        if (ids.empty()) return std::nullopt;

        std::lock_guard lock{stateMutex};
        GeoEntry        result;
        for (auto id : ids) {
            geoPackets.erase_if(id, [this, id, &result](auto&& iter) {
                removeChunkReferencesLocked(id, iter.second);
                for (auto& packet : iter.second.packets) {
                    result.packets.emplace_back(std::move(packet));
                }
                return true;
            });
        }

        if (result.packets.empty()) return std::nullopt;
        return result;
    }

    void addGeoEntry(GeoId id, GeoEntry entry) {
        if (entry.packets.empty()) return;

        std::lock_guard lock{stateMutex};
        addChunkReferencesLocked(id, entry);
        geoPackets.emplace(id, std::move(entry));
    }

    bool shift(GeoId id, Vec3 const& offset, std::vector<SendTarget>& targets) {
        std::lock_guard lock{stateMutex};
        bool            found = false;

        geoPackets.modify_if(id, [this, id, &offset, &targets, &found](auto&& iter) {
            found       = true;
            auto& entry = iter.second;

            removeChunkReferencesLocked(id, entry);
            for (auto& packet : entry.packets) {
                shiftPacket(*packet.packet, offset);
                packet.position += offset;
                addChunkReferenceLocked(id, packet);
                targets.push_back(SendTarget{packet.packet, packet.position, packet.dimension});
            }
        });

        return found;
    }
};

static std::mutex                        listMutex;
static std::vector<DebugDrawingHandler*> handlers;
static std::atomic_bool                  hasInstance{false};

LL_TYPE_INSTANCE_HOOK(
    DebugDrawingHandler::State::Hook,
    ll::memory::HookPriority::Normal,
    LoopbackPacketSender,
    &LoopbackPacketSender::$sendToClient,
    void,
    ::NetworkIdentifier const& id,
    ::Packet const&            packet,
    ::SubClientId              recipientSubId
) {
    origin(id, packet, recipientSubId);

    if (!hasInstance || packet.getId() != MinecraftPacketIds::FullChunkData) return;

    auto const& levelChunkPacket = static_cast<LevelChunkPacket const&>(packet);
    auto const  key =
        std::make_pair(levelChunkPacket.mPos, static_cast<int>(*levelChunkPacket.mDimensionId));

    std::vector<PacketPtr> packets;
    {
        std::lock_guard lock{listMutex};
        for (auto* handler : handlers) {
            auto chunkPackets = handler->drawingState->packetsForChunk(key);
            packets.insert(
                packets.end(),
                std::make_move_iterator(chunkPackets.begin()),
                std::make_move_iterator(chunkPackets.end())
            );
        }
    }

    for (auto const& debugPacket : packets) {
        debugPacket->sendToClient(id, recipientSubId);
    }
};

DebugDrawingHandler::DebugDrawingHandler() : drawingState(std::make_unique<State>()) {
    static ll::memory::HookRegistrar<DebugDrawingHandler::State::Hook> registrar;

    std::lock_guard lock{listMutex};
    handlers.push_back(this);
    hasInstance = true;
}

DebugDrawingHandler::~DebugDrawingHandler() {
    std::lock_guard lock{listMutex};

    auto it = std::find(handlers.begin(), handlers.end(), this);
    if (it != handlers.end()) {
        *it = handlers.back();
        handlers.pop_back();
    }
    hasInstance = !handlers.empty();
}

GroupType DebugDrawingHandler::type() const noexcept {
    return GroupType::DebugDrawing;
}

GeometryGroup::GeoId DebugDrawingHandler::line(
    DimensionType     dim,
    Vec3 const&       begin,
    Vec3 const&       end,
    mce::Color const& color,
    std::optional<float> /*thickness*/
) {
    if (begin == end) return GeoId::invalid();

    Vec3   offset = end - begin;
    double len    = offset.length();
    if (len <= shapeDisplayRadius + 0.5) {
        PrimitiveShape shape;
        shape.mNetworkId        = nextId_.fetch_sub(1);
        shape.mShapeType        = ::ScriptModuleMinecraft::ScriptPrimitiveShapeType::Line;
        shape.mLocation         = begin;
        shape.mColor            = color;
        shape.mDimensionId      = dim;
        shape.mExtraDataPayload = LineDataPayload{.mEndLocation = end};

        auto packet = makePacket(std::move(shape));
        auto id     = getNextGeoId();
        drawingState->addPacket(id, packet, dim, begin);
        sendPacket(id, packet, begin, dim);
        return id;
    }

    int segmentNum  = static_cast<int>(len) / shapeDisplayRadius + 1;
    offset         /= segmentNum;

    std::vector<GeoId> ids;
    ids.reserve(segmentNum);
    Vec3 currentPos = begin;
    Vec3 lastPos    = begin;
    for (int i = 0; i < segmentNum - 1; ++i) {
        currentPos += offset;
        ids.emplace_back(line(dim, lastPos, currentPos, color));
        lastPos = currentPos;
    }
    ids.emplace_back(line(dim, currentPos, end, color));
    return merge(ids);
}

GeometryGroup::GeoId DebugDrawingHandler::box(
    DimensionType        dim,
    AABB const&          box,
    mce::Color const&    color,
    std::optional<float> thickness
) {
    if ((box.max - box.min).lengthSqr() >= shapeDisplayRadius * shapeDisplayRadius)
        return Base::box(dim, box, color, thickness);

    auto center = (box.min + box.max) / 2;

    PrimitiveShape shape;
    shape.mNetworkId        = nextId_.fetch_sub(1);
    shape.mShapeType        = ::ScriptModuleMinecraft::ScriptPrimitiveShapeType::Box;
    shape.mLocation         = center;
    shape.mColor            = color;
    shape.mDimensionId      = dim;
    shape.mExtraDataPayload = BoxDataPayload{.mBoxBound = box.max - box.min};

    auto packet = makePacket(std::move(shape));
    auto id     = getNextGeoId();
    drawingState->addPacket(id, packet, dim, center);
    sendPacket(id, packet, center, dim);
    return id;
}

GeometryGroup::GeoId DebugDrawingHandler::circle(
    DimensionType        dim,
    Vec3 const&          center,
    Vec3 const&          normal,
    float                radius,
    mce::Color const&    color,
    std::optional<float> thickness
) {
    auto const& config = getBSCI().getConfig().debugDraw;
    if (radius > shapeDisplayRadius || !config.useNativeCircle)
        return Base::circle(dim, center, normal, radius, color, thickness);

    PrimitiveShape shape;
    shape.mNetworkId   = nextId_.fetch_sub(1);
    shape.mShapeType   = ::ScriptModuleMinecraft::ScriptPrimitiveShapeType::Circle;
    shape.mRotation    = normal;
    shape.mLocation    = center;
    shape.mScale       = radius;
    shape.mColor       = color;
    shape.mDimensionId = dim;

    auto packet = makePacket(std::move(shape));
    auto id     = getNextGeoId();
    drawingState->addPacket(id, packet, dim, center);
    sendPacket(id, packet, center, dim);
    return id;
}

GeometryGroup::GeoId DebugDrawingHandler::sphere(
    DimensionType        dim,
    Vec3 const&          center,
    float                radius,
    mce::Color const&    color,
    std::optional<float> thickness
) {
    auto const& config = getBSCI().getConfig().debugDraw;
    if (radius > shapeDisplayRadius || !config.useNativeSphere)
        return Base::sphere(dim, center, radius, color, thickness);

    PrimitiveShape shape;
    shape.mNetworkId   = nextId_.fetch_sub(1);
    shape.mShapeType   = ::ScriptModuleMinecraft::ScriptPrimitiveShapeType::Sphere;
    shape.mLocation    = center;
    shape.mScale       = radius;
    shape.mColor       = color;
    shape.mDimensionId = dim;
    if (config.sphereSegments.has_value()) {
        shape.mExtraDataPayload = SphereDataPayload{.mNumSegments = config.sphereSegments.value()};
    }

    auto packet = makePacket(std::move(shape));
    auto id     = getNextGeoId();
    drawingState->addPacket(id, packet, dim, center);
    sendPacket(id, packet, center, dim);
    return id;
}

GeometryGroup::GeoId DebugDrawingHandler::arrow(
    DimensionType        dim,
    Vec3 const&          begin,
    Vec3 const&          end,
    mce::Color const&    color,
    std::optional<float> mArrowHeadLength,
    std::optional<float> mArrowHeadRadius
) {
    if (begin == end) return GeoId::invalid();

    Vec3   offset = end - begin;
    double len    = offset.length();
    if (len <= shapeDisplayRadius + 0.5) {
        auto const& config = getBSCI().getConfig().debugDraw;

        PrimitiveShape shape;
        shape.mNetworkId        = nextId_.fetch_sub(1);
        shape.mShapeType        = ::ScriptModuleMinecraft::ScriptPrimitiveShapeType::Arrow;
        shape.mLocation         = begin;
        shape.mColor            = color;
        shape.mDimensionId      = dim;
        shape.mExtraDataPayload = ArrowDataPayload{
            .mEndLocation     = end,
            .mArrowHeadLength = mArrowHeadLength,
            .mArrowHeadRadius = mArrowHeadRadius,
            .mNumSegments     = config.arrowSegments
        };

        auto packet = makePacket(std::move(shape));
        auto id     = getNextGeoId();
        drawingState->addPacket(id, packet, dim, begin);
        sendPacket(id, packet, begin, dim);
        return id;
    }

    int segmentNum  = static_cast<int>(len) / shapeDisplayRadius + 1;
    offset         /= segmentNum;

    std::vector<GeoId> ids;
    ids.reserve(segmentNum);
    Vec3 currentPos = begin;
    Vec3 lastPos    = begin;
    for (int i = 0; i < segmentNum - 1; ++i) {
        currentPos += offset;
        ids.emplace_back(line(dim, lastPos, currentPos, color));
        lastPos = currentPos;
    }
    ids.emplace_back(arrow(dim, currentPos, end, color, mArrowHeadLength, mArrowHeadRadius));
    return merge(ids);
}

GeometryGroup::GeoId DebugDrawingHandler::text(
    DimensionType        dim,
    Vec3 const&          pos,
    std::string          text,
    mce::Color const&    color,
    std::optional<float> scale
) {
    PrimitiveShape shape;
    shape.mNetworkId   = nextId_.fetch_sub(1);
    shape.mShapeType   = ::ScriptModuleMinecraft::ScriptPrimitiveShapeType::Text;
    shape.mLocation    = pos;
    shape.mScale       = scale;
    shape.mColor       = color;
    shape.mDimensionId = dim;

    TextDataPayload textPayload;
    textPayload.mText       = std::move(text);
    shape.mExtraDataPayload = std::move(textPayload);

    auto packet = makePacket(std::move(shape));
    auto id     = getNextGeoId();
    drawingState->addPacket(id, packet, dim, pos);
    sendPacket(id, packet, pos, dim);
    return id;
}

bool DebugDrawingHandler::remove(GeoId id) {
    if (id.value == 0) return false;

    auto removed = drawingState->take(std::span<GeoId const>{&id, 1});
    if (!removed.has_value()) return false;

    for (auto& entry : removed->packets) {
        auto packet = std::move(entry.packet);
        if (!packet) continue;
        for (auto& shape : *packet->mShapes) shape.mShapeType = std::nullopt;
        sendPacket(
            id,
            std::move(packet),
            entry.position,
            entry.dimension,
            PacketSendMode::Broadcast
        );
    }
    return true;
}

GeometryGroup::GeoId DebugDrawingHandler::merge(std::span<GeoId> ids) {
    if (ids.empty()) return GeoId::invalid();

    std::span<GeoId const> constIds{ids.data(), ids.size()};
    auto                   merged = drawingState->take(constIds);
    if (!merged.has_value()) return GeoId::invalid();

    auto newId = getNextGeoId();
    drawingState->addGeoEntry(newId, std::move(*merged));
    return newId;
}

bool DebugDrawingHandler::shift(GeoId id, Vec3 const& offset) {
    if (id.value == 0) return false;

    std::vector<State::SendTarget> targets;
    if (!drawingState->shift(id, offset, targets)) return false;

    for (auto const& target : targets) {
        sendPacket(id, target.packet, target.position, target.dimension);
    }
    return true;
}

} // namespace bsci
