#include "DebugDrawingPacketHandler.h"
#include "BedrockServerClientInterface.h"
#include "bsci/particle/ParticleSpawner.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/NetEventCallback.h"
#include "mc/network/packet/DebugDrawerPacket.h"
#include "mc/network/packet/DebugDrawerPacketPayload.h"
#include "mc/network/packet/LevelChunkPacket.h"
#include "mc/network/packet/ShapeDataPayload.h"
#include "mc/world/level/ChunkPos.h"
#include <memory>
#include <parallel_hashmap/phmap.h>


TextDataPayload::TextDataPayload() = default;
TextDataPayload::TextDataPayload(TextDataPayload const& cp) { mText = cp.mText; };
// ShapeDataPayload::ShapeDataPayload()                                                = default;
ShapeDataPayload::ShapeDataPayload() { mNetworkId = 0; };
DebugDrawerPacketPayload::DebugDrawerPacketPayload()                                = default;
DebugDrawerPacketPayload::DebugDrawerPacketPayload(DebugDrawerPacketPayload const&) = default;

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
// std::unique_ptr<GeometryGroup> GeometryGroup::createDefault() {
//     return std::make_unique<DebugDrawingPacketHandler>();
// }

static std::atomic<uint64_t> nextId_{UINT64_MAX};

class DebugDrawingPacketHandler::Impl {
public:
    struct Hook;
    size_t                           id{};
    std::unique_ptr<ParticleSpawner> particleSpawner =
        std::make_unique<ParticleSpawner>(); // 向前兼容
    ph_flat_hash_map<GeoId, std::shared_ptr<DebugDrawerPacket>, 6>
        geoPackets; // ptr = nullptr 表示使用ParticleSpawner
    ph_flat_hash_map<GeoId, std::pair<std::vector<GeoId>, bool>>
        geoGroup; // bool = true 表示启用ParticleSpawner
    ph_flat_hash_map<ChunkPos, std::vector<GeoId>[3]>
        chunkPackets; // 不应包含使用ParticleSpawner的GeoId

public:
    void sendPacketImmediately(DebugDrawerPacket& pkt) {
        if (pkt.mShapes->empty()) return;
        ll::thread::ServerThreadExecutor::getDefault().execute([pkt] { pkt.sendToClients(); });
    }
};

static std::recursive_mutex                    listMutex;
static std::vector<DebugDrawingPacketHandler*> list;
static std::atomic_bool                        hasInstance{false};

LL_TYPE_INSTANCE_HOOK(
    DebugDrawingPacketHandler::Impl::Hook,
    ll::memory::HookPriority::Normal,
    LoopbackPacketSender,
    &LoopbackPacketSender::$sendToClient,
    void,
    ::NetworkIdentifier const& id,
    ::Packet const&            packet,
    ::SubClientId              recipientSubId
) {
    if (packet.getId() == MinecraftPacketIds::FullChunkData && hasInstance) {
        std::vector<std::shared_ptr<DebugDrawerPacket>> chunkPackets;

        const auto& levelChunkPacket = static_cast<LevelChunkPacket const&>(packet);
        const auto& chunkPos         = levelChunkPacket.mPos;
        const auto& dimId            = levelChunkPacket.mDimensionId;

        {
            std::lock_guard l{listMutex};

            chunkPackets.reserve(list.size());
            for (auto s : list) {
                s->impl->chunkPackets.if_contains(
                    chunkPos,
                    [&s, &chunkPackets, &dimId](auto&& iter) {
                        for (auto& subId : iter.second[dimId]) {
                            s->impl->geoPackets.if_contains(subId, [&chunkPackets](auto&& it) {
                                chunkPackets.push_back(it.second);
                            });
                        }
                    }
                );
            }
        }
        BedrockServerClientInterface::getInstance().getSelf().getLogger().warn(
            "sendToChunk: " + chunkPos->toString()
        );
        for (auto& pkt : chunkPackets) {
            if (pkt) pkt->sendToClient(id, recipientSubId);
        }
    }
    origin(id, packet, recipientSubId);
};

DebugDrawingPacketHandler::DebugDrawingPacketHandler() : impl(std::make_unique<Impl>()) {
    static ll::memory::HookRegistrar<DebugDrawingPacketHandler::Impl::Hook> reg;
    std::lock_guard                                                         l{listMutex};
    hasInstance = true;
    impl->id    = list.size();
    list.push_back(this);
}

DebugDrawingPacketHandler::~DebugDrawingPacketHandler() {
    std::lock_guard l{listMutex};
    list.back()->impl->id = impl->id;
    std::swap(list[impl->id], list.back());
    list.pop_back();
    hasInstance = !list.empty();
}

GeometryGroup::GeoId DebugDrawingPacketHandler::point(
    DimensionType        dim,
    Vec3 const&          pos,
    mce::Color const&    color,
    std::optional<float> radius
) {
    auto geoId = this->impl->particleSpawner->point(dim, pos, color, radius);
    this->impl->geoPackets.emplace(geoId, nullptr);
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
        auto geoId = this->impl->particleSpawner->line(dim, begin, end, color, thickness);
        this->impl->geoPackets.emplace(geoId, nullptr);
        return geoId;
    }

    Vec3   offset = end - begin;
    double len    = offset.length();
    if (len <= 48) {
        auto packet = std::make_shared<DebugDrawerPacket>();
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
        this->impl->geoPackets.emplace(id, std::move(packet));
        return id;
    }

    int segmentNum  = ((int)len) / 48 + 1;
    offset         /= segmentNum;
    BedrockServerClientInterface::getInstance().getSelf().getLogger().warn(
        "切分数：" + std::to_string(segmentNum)
    );
    Vec3 currentPos = begin;
    Vec3 lastPos    = begin;

    std::vector<GeoId> ids;
    ids.reserve(segmentNum);
    for (int i = 0; i < segmentNum - 1; i++) {
        currentPos += offset;
        ids.emplace_back(this->line(dim, lastPos, currentPos, color));
        lastPos = currentPos;
    }
    ids.emplace_back(this->line(dim, currentPos, end, color)); // 避免浮点误差

    return this->merge(ids);
}
} // namespace bsci