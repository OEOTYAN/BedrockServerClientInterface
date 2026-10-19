#include "DebugDrawingHandler.h"
#include "BedrockServerClientInterface.h"
#include "bsci/utils/Utils.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <ll/api/base/Containers.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/thread/ServerThreadExecutor.h>

#include <mc/legacy/ActorUniqueID.h>
#include <mc/network/LoopbackPacketSender.h>
#include <mc/network/MinecraftPacketIds.h>
#include <mc/network/NetEventCallback.h>
#include <mc/network/NetworkIdentifierWithSubId.h>
#include <mc/network/packet/LevelChunkPacket.h>
#include <mc/network/packet/PrimitiveShapeDataPayload.h>
#include <mc/network/packet/PrimitiveShapesPacket.h>
#include <mc/network/packet/PrimitiveShapesPacketPayload.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/level/ChunkPos.h>
#include <mc/world/level/Level.h>

namespace bsci {
static std::atomic<uint64_t> nextId_{UINT64_MAX};

constexpr size_t shapeDisplayRadius = 48;

class DebugDrawingHandler::Impl {
public:
    using PacketPair = std::pair<GeoId, std::vector<std::weak_ptr<PrimitiveShapesPacket>>>;

    struct Targets {
        std::vector<ActorUniqueID>              players;
        std::vector<NetworkIdentifierWithSubId> clients;

        [[nodiscard]] bool broadcast() const { return players.empty(); }
    };

    struct Hook;
    size_t id{};
    ll::ConcurrentDenseMap<GeoId, std::vector<std::shared_ptr<PrimitiveShapesPacket>>>
        geoPackets; // bool = true 表示启用ParticleSpawner
    ll::ConcurrentDenseMap<std::pair<ChunkPos, int>, std::vector<PacketPair>>
        chunkPackets; // 不应包含使用ParticleSpawner的GeoId
    ll::ConcurrentDenseMap<GeoId, Targets> targets;

public:
    static Targets makeTargets(std::vector<Player*> const& players) {
        Targets result;
        result.players.reserve(players.size());
        result.clients.reserve(players.size());
        for (auto* player : players) {
            if (!player) continue;
            result.players.emplace_back(player->getOrCreateUniqueID());
            result.clients.emplace_back(NetworkIdentifierWithSubId{
                player->getNetworkIdentifier(),
                player->getClientSubId()
            });
        }
        return result;
    }

    static void mergeTargets(Targets& dst, Targets const& src) {
        for (auto const& id : src.players) {
            bool exists = false;
            for (auto const& other : dst.players) {
                if (other == id) {
                    exists = true;
                    break;
                }
            }
            if (!exists) dst.players.emplace_back(id);
        }
        for (auto const& client : src.clients) {
            bool exists = false;
            for (auto const& other : dst.clients) {
                if (other.id == client.id && other.subClientId == client.subClientId) {
                    exists = true;
                    break;
                }
            }
            if (!exists) dst.clients.emplace_back(client);
        }
    }

    static void sendPacket(
        PrimitiveShapesPacket const& packet,
        DimensionType                dim,
        Vec3 const&                  pos,
        Targets const&               targets
    ) {
        if (targets.broadcast()) {
            packet.sendTo(pos, dim);
            return;
        }

        auto level = ll::service::getLevel();
        if (!level) return;
        auto dimension = level->getDimension(dim).lock();
        if (!dimension) return;

        std::vector<Player*> players;
        players.reserve(targets.players.size());
        for (auto const& id : targets.players) {
            if (auto* player = level->getPlayer(id)) players.emplace_back(player);
        }
        if (players.empty()) return;
        sendToPlayerAt(*dimension, pos, packet, players);
    }

    [[nodiscard]] bool
    isVisibleTo(GeoId geoId, NetworkIdentifier const& id, SubClientId subClientId) const {
        auto it = targets.find(geoId);
        if (it == targets.end()) return true;
        if (it->second.broadcast()) return true;
        for (auto const& client : it->second.clients) {
            if (client.id == id && client.subClientId == subClientId) return true;
        }
        return false;
    }

    // void sendPacketImmediately(DebugDrawerPacket& pkt) {
    //     if (pkt.mShapes->empty()) return;
    //     ll::thread::ServerThreadExecutor::getDefault().execute([pkt] { pkt.sendToClients(); });
    // }

    static bool compareByGeoId(const PacketPair& a, const GeoId& b) {
        return a.first.value < b.value;
    }

    void addPacket(
        GeoId                                   geoId,
        std::shared_ptr<PrimitiveShapesPacket>& packet,
        int                                     dimId,
        Vec3 const&                             pos,
        Targets const&                          target
    ) {
        if (packet) {
            geoPackets.emplace(geoId, std::vector<std::shared_ptr<PrimitiveShapesPacket>>{packet});
            targets.emplace(geoId, target);
            auto chunkPos         = ChunkPos(pos);
            auto key              = std::make_pair(chunkPos, dimId);
            auto [iter, inserted] = chunkPackets.try_emplace(key);

            if (inserted) {
                iter->second.emplace_back(
                    geoId,
                    std::vector<std::weak_ptr<PrimitiveShapesPacket>>{packet}
                );
            } else {
                auto it = std::lower_bound(
                    iter->second.begin(),
                    iter->second.end(),
                    geoId,
                    compareByGeoId
                );
                iter->second
                    .emplace(it, geoId, std::vector<std::weak_ptr<PrimitiveShapesPacket>>{packet});
            }
        }
        // else {
        //     geoGroup.emplace(
        //         geoId,
        //         std::make_pair(std::vector<std::shared_ptr<PrimitiveShapesPacket>>{}, true)
        //     );
        // }
    }
};

static std::recursive_mutex              listMutex;
static std::vector<DebugDrawingHandler*> list;
static std::atomic_bool                  hasInstance{false};

LL_TYPE_INSTANCE_HOOK(
    DebugDrawingHandler::Impl::Hook,
    ll::memory::HookPriority::Normal,
    LoopbackPacketSender,
    &LoopbackPacketSender::$sendToClient,
    void,
    ::NetworkIdentifier const& id,
    ::Packet const&            packet,
    ::SubClientId              recipientSubId
) {
    if (packet.getId() == MinecraftPacketIds::FullChunkData && hasInstance) [[unlikely]] {

        const auto& levelChunkPacket = static_cast<LevelChunkPacket const&>(packet);
        const auto& chunkPos         = levelChunkPacket.mPos;
        const auto& dimId            = (int)*levelChunkPacket.mDimensionId;
        auto        key              = std::make_pair(chunkPos, dimId);

        std::vector<std::shared_ptr<PrimitiveShapesPacket>> _chunkPackets;

        {
            std::lock_guard l{listMutex};

            for (auto s : list) {
                s->impl->chunkPackets.erase_if(
                    key,
                    [&_chunkPackets, s, &id, recipientSubId](auto&& iter) {
                        std::erase_if(
                            iter.second,
                            [&_chunkPackets, s, &id, recipientSubId](auto&& packetPair) {
                                _chunkPackets.reserve(
                                    _chunkPackets.size() + packetPair.second.size()
                                );
                                std::erase_if(
                                    packetPair.second,
                                    [&_chunkPackets, s, &packetPair, &id, recipientSubId](
                                        auto&& pkt
                                    ) {
                                        if (auto shared = pkt.lock()) {
                                            if (s->impl->isVisibleTo(
                                                    packetPair.first,
                                                    id,
                                                    recipientSubId
                                                )) {
                                                _chunkPackets.emplace_back(shared);
                                            }
                                            return false;
                                        }
                                        return true;
                                    }
                                );
                                return packetPair.second.empty();
                            }
                        );
                        return iter.second.empty();
                    }
                );
            }
        }
        for (auto& pkt : _chunkPackets) {
            if (pkt) pkt->sendToClient(id, recipientSubId);
        }
    }
    origin(id, packet, recipientSubId);
};

DebugDrawingHandler::DebugDrawingHandler() : impl(std::make_unique<Impl>()) {
    static ll::memory::HookRegistrar<DebugDrawingHandler::Impl::Hook> reg;
    std::lock_guard                                                   l{listMutex};
    hasInstance = true;
    impl->id    = list.size();
    list.push_back(this);
}

DebugDrawingHandler::~DebugDrawingHandler() {
    std::lock_guard l{listMutex};
    list.back()->impl->id = impl->id;
    std::swap(list[impl->id], list.back());
    list.pop_back();
    hasInstance = !list.empty();
}

GeometryGroup::GeoId DebugDrawingHandler::line(
    DimensionType               dim,
    Vec3 const&                 begin,
    Vec3 const&                 end,
    mce::Color const&           color,
    std::optional<float> /*thickness*/,
    std::vector<Player*> const& players
) {
    if (begin == end) return GeoId::invalid();

    auto targets = Impl::makeTargets(players);

    Vec3   offset = end - begin;
    double len    = offset.length();
    if (len <= shapeDisplayRadius + 0.5) { // 防止浮点误差导致的无限递归
        auto packet = std::make_shared<PrimitiveShapesPacket>();
        packet->setSerializationMode(SerializationMode::CerealOnly);
        PrimitiveShapeDataPayload shape;
        shape.mNetworkId        = nextId_.fetch_sub(1);
        shape.mShapeType        = ::ScriptModuleMinecraft::ScriptPrimitiveShapeType::Line;
        shape.mLocation         = begin;
        shape.mColor            = color;
        shape.mDimensionId      = dim;
        shape.mExtraDataPayload = LineDataPayload{.mEndLocation = end};
        packet->mShapes->emplace_back(std::move(shape));
        ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin, dim, targets] {
            Impl::sendPacket(*packet, dim, begin, targets);
        });

        auto id = getNextGeoId();
        impl->addPacket(id, packet, dim, begin, targets);
        return id;
    }

    int segmentNum   = ((int)len) / shapeDisplayRadius + 1;
    offset          /= segmentNum;
    Vec3 currentPos  = begin;
    Vec3 lastPos     = begin;

    std::vector<GeoId> ids;
    ids.reserve(segmentNum);
    for (int i = 0; i < segmentNum - 1; i++) {
        currentPos += offset;
        ids.emplace_back(line(dim, lastPos, currentPos, color, {}, players));
        lastPos = currentPos;
    }
    ids.emplace_back(line(dim, currentPos, end, color, {}, players)); // 避免浮点误差

    return merge(ids);
}

GeometryGroup::GeoId DebugDrawingHandler::box(
    DimensionType               dim,
    AABB const&                 box,
    mce::Color const&           color,
    std::optional<float>        thickness,
    std::vector<Player*> const& players
) {
    if ((box.max - box.min).lengthSqr() >= shapeDisplayRadius * shapeDisplayRadius)
        return Base::box(dim, box, color, thickness, players);

    auto targets = Impl::makeTargets(players);

    auto packet = std::make_shared<PrimitiveShapesPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    PrimitiveShapeDataPayload shape;
    shape.mNetworkId        = nextId_.fetch_sub(1);
    shape.mShapeType        = ::ScriptModuleMinecraft::ScriptPrimitiveShapeType::Box;
    shape.mLocation         = (box.min + box.max) / 2;
    shape.mColor            = color;
    shape.mDimensionId      = dim;
    shape.mExtraDataPayload = BoxDataPayload{.mBoxBound = box.max - box.min};
    packet->mShapes->emplace_back(std::move(shape));
    ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin = box.min, dim, targets] {
        Impl::sendPacket(*packet, dim, begin, targets);
    });

    auto id = getNextGeoId();
    impl->addPacket(id, packet, dim, box.min, targets);
    return id;
}


GeometryGroup::GeoId DebugDrawingHandler::circle(
    DimensionType               dim,
    Vec3 const&                 center,
    Vec3 const&                 normal,
    float                       radius,
    mce::Color const&           color,
    std::optional<float>        thickness,
    std::vector<Player*> const& players
) {
    auto const& config = BedrockServerClientInterface::getInstance().getConfig().debugDraw;

    if (radius > shapeDisplayRadius || !config.useNativeCircle) {
        return Base::circle(dim, center, normal, radius, color, thickness, players);
    }

    auto targets = Impl::makeTargets(players);

    auto packet = std::make_shared<PrimitiveShapesPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    PrimitiveShapeDataPayload shape;
    shape.mNetworkId   = nextId_.fetch_sub(1);
    shape.mShapeType   = ::ScriptModuleMinecraft::ScriptPrimitiveShapeType::Circle;
    shape.mRotation    = normal;
    shape.mLocation    = center;
    shape.mScale       = radius;
    shape.mColor       = color;
    shape.mDimensionId = dim;
    packet->mShapes->emplace_back(std::move(shape));
    ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin = center, dim, targets] {
        Impl::sendPacket(*packet, dim, begin, targets);
    });

    auto id = getNextGeoId();
    impl->addPacket(id, packet, dim, center, targets);
    return id;
}

GeometryGroup::GeoId DebugDrawingHandler::sphere(
    DimensionType               dim,
    Vec3 const&                 center,
    float                       radius,
    mce::Color const&           color,
    std::optional<float>        thickness,
    std::vector<Player*> const& players
) {
    auto const& config = BedrockServerClientInterface::getInstance().getConfig().debugDraw;

    if (radius > shapeDisplayRadius || !config.useNativeSphere) {
        return Base::sphere(dim, center, radius, color, thickness, players);
    }

    auto targets = Impl::makeTargets(players);


    auto packet = std::make_shared<PrimitiveShapesPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    PrimitiveShapeDataPayload shape;
    shape.mNetworkId   = nextId_.fetch_sub(1);
    shape.mShapeType   = ::ScriptModuleMinecraft::ScriptPrimitiveShapeType::Sphere;
    shape.mLocation    = center;
    shape.mScale       = radius;
    shape.mColor       = color;
    shape.mDimensionId = dim;
    if (config.sphereSegments.has_value()) {
        shape.mExtraDataPayload = SphereDataPayload{.mNumSegments = config.sphereSegments.value()};
    }
    packet->mShapes->emplace_back(std::move(shape));
    ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin = center, dim, targets] {
        Impl::sendPacket(*packet, dim, begin, targets);
    });

    auto id = getNextGeoId();
    impl->addPacket(id, packet, dim, center, targets);
    return id;
}

GeometryGroup::GeoId DebugDrawingHandler::arrow(
    DimensionType               dim,
    Vec3 const&                 begin,
    Vec3 const&                 end,
    mce::Color const&           color,
    std::optional<float>        mArrowHeadLength,
    std::optional<float>        mArrowHeadRadius,
    std::vector<Player*> const& players
) {
    if (begin == end) return GeoId::invalid();

    auto targets = Impl::makeTargets(players);

    Vec3   offset = end - begin;
    double len    = offset.length();
    if (len <= shapeDisplayRadius + 0.5) { // 防止浮点误差导致的无限递归
        auto packet = std::make_shared<PrimitiveShapesPacket>();
        packet->setSerializationMode(SerializationMode::CerealOnly);
        auto const&      config = BedrockServerClientInterface::getInstance().getConfig().debugDraw;
        PrimitiveShapeDataPayload shape;
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
        packet->mShapes->emplace_back(std::move(shape));
        ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin, dim, targets] {
            Impl::sendPacket(*packet, dim, begin, targets);
        });

        auto id = getNextGeoId();
        impl->addPacket(id, packet, dim, begin, targets);
        return id;
    }

    int segmentNum                 = ((int)len) / shapeDisplayRadius + 1;
    offset                        /= segmentNum;
    Vec3               currentPos  = begin;
    Vec3               lastPos     = begin;
    std::vector<GeoId> ids;
    ids.reserve(segmentNum);
    for (int i = 0; i < segmentNum - 1; i++) {
        currentPos += offset;
        ids.emplace_back(line(dim, lastPos, currentPos, color, {}, players));
        lastPos = currentPos;
    }
    ids.emplace_back(
        arrow(dim, currentPos, end, color, mArrowHeadLength, mArrowHeadRadius, players)
    );
    return merge(ids);
}

GeometryGroup::GeoId DebugDrawingHandler::text(
    DimensionType               dim,
    Vec3 const&                 pos,
    std::string                 text,
    mce::Color const&           color,
    std::optional<float>        scale,
    std::vector<Player*> const& players
) {
    auto targets = Impl::makeTargets(players);

    auto packet = std::make_shared<PrimitiveShapesPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    PrimitiveShapeDataPayload shape;
    shape.mNetworkId   = nextId_.fetch_sub(1);
    shape.mShapeType   = ::ScriptModuleMinecraft::ScriptPrimitiveShapeType::Text;
    shape.mLocation    = pos;
    shape.mScale       = scale;
    shape.mColor       = color;
    shape.mDimensionId = dim;
    TextDataPayload extraDataPayload;
    extraDataPayload.mText  = std::move(text);
    shape.mExtraDataPayload = std::move(extraDataPayload);
    packet->mShapes->emplace_back(std::move(shape));
    ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin = pos, dim, targets] {
        Impl::sendPacket(*packet, dim, begin, targets);
    });

    auto id = getNextGeoId();
    impl->addPacket(id, packet, dim, pos, targets);
    return id;
}

bool DebugDrawingHandler::remove(GeoId id) {
    if (id.value == 0) {
        return false;
    }
    std::vector<std::shared_ptr<PrimitiveShapesPacket>> removePackets;
    impl->geoPackets.erase_if(id, [this, id, &removePackets](auto&& iter) {
        for (auto& packet : iter.second) {
            if (packet && !packet->mShapes->empty() && (*packet->mShapes)[0].mLocation->has_value()
                && (*packet->mShapes)[0].mDimensionId->has_value()) {
                auto chunkPos = ChunkPos((*packet->mShapes)[0].mLocation->value());
                auto dimId    = (int)(*packet->mShapes)[0].mDimensionId->value();
                auto key      = std::make_pair(chunkPos, dimId);
                impl->chunkPackets.erase_if(key, [this, id](auto&& iter) {
                    auto it = std::lower_bound(
                        iter.second.begin(),
                        iter.second.end(),
                        id,
                        impl->compareByGeoId
                    );
                    if (it != iter.second.end() && it->first.value == id.value) {
                        iter.second.erase(it);
                    }
                    return iter.second.empty();
                });
            }
        }
        removePackets = std::move(iter.second);
        return true;
    });

    Impl::Targets target;
    if (auto it = impl->targets.find(id); it != impl->targets.end()) target = it->second;
    impl->targets.erase(id);

    for (auto& packet : removePackets) {
        ll::thread::ServerThreadExecutor::getDefault().execute([packet, target] {
            if (packet) {
                for (auto& shape : *packet->mShapes) shape.mShapeType = std::nullopt;
                if (target.broadcast()) {
                    packet->sendToClients();
                } else {
                    for (auto const& client : target.clients) packet->sendToClient(client);
                }
            }
        });
    }
    return true;
}

GeometryGroup::GeoId DebugDrawingHandler::merge(std::span<GeoId> ids) {
    if (ids.empty()) {
        return GeoId::invalid();
    }
    ll::ConcurrentDenseMap<std::pair<ChunkPos, int>,
                           std::vector<GeoId>>      temMap;     // 用来处理包的合并
    std::vector<std::shared_ptr<PrimitiveShapesPacket>> newPackets; // 合并后的包队列

    // 移出旧id的geoPackets
    for (auto& id : ids) {
        impl->geoPackets.erase_if(id, [id, &temMap, &newPackets](auto&& iter) {
            // 处理包的合并
            std::erase_if(iter.second, [&temMap, id](auto&& packet) {
                if (packet && !packet->mShapes->empty()
                    && (*packet->mShapes)[0].mLocation->has_value()
                    && (*packet->mShapes)[0].mDimensionId->has_value()) {
                    auto chunkPos         = ChunkPos((*packet->mShapes)[0].mLocation->value());
                    auto dimId            = (int)(*packet->mShapes)[0].mDimensionId->value();
                    auto key              = std::make_pair(chunkPos, dimId);
                    auto [iter, inserted] = temMap.try_emplace(key);

                    // 记录对应区块待合并的GeoId
                    iter->second.emplace_back(id);
                    return false;
                } else return true;
            });
            newPackets.insert(
                newPackets.end(),
                std::make_move_iterator(iter.second.begin()),
                std::make_move_iterator(iter.second.end())
            );
            return true;
        });
    }

    if (newPackets.size() == 0) return GeoId::invalid();
    auto newId = getNextGeoId();

    // 处理chunkPackets
    for (auto& [key, data] : temMap) {
        impl->chunkPackets.modify_if(key, [this, &data, &newId](auto&& iter) {
            // 合并包
            std::vector<std::weak_ptr<PrimitiveShapesPacket>> pkts;
            for (auto& id : data) {
                auto it = std::lower_bound(
                    iter.second.begin(),
                    iter.second.end(),
                    id,
                    impl->compareByGeoId
                );
                if (it != iter.second.end() && it->first.value == id.value) {
                    pkts.insert(
                        pkts.end(),
                        std::make_move_iterator(it->second.begin()),
                        std::make_move_iterator(it->second.end())
                    );
                    iter.second.erase(it);
                }
            }
            // 添加新的包
            std::pair<GeoId, std::vector<std::weak_ptr<PrimitiveShapesPacket>>> newPair =
                std::make_pair(newId, std::move(pkts));
            auto it = std::lower_bound(
                iter.second.begin(),
                iter.second.end(),
                newId,
                impl->compareByGeoId
            );
            iter.second.insert(it, std::move(newPair));
        });
    }

    // 添加新id的geoPackets
    impl->geoPackets.emplace(newId, std::move(newPackets));

    // 合并目标玩家
    Impl::Targets mergedTargets;
    bool          broadcast = false;
    for (auto id : ids) {
        auto it = impl->targets.find(id);
        if (it == impl->targets.end() || it->second.broadcast()) {
            broadcast = true;
            break;
        }
        Impl::mergeTargets(mergedTargets, it->second);
    }
    for (auto id : ids) impl->targets.erase(id);
    impl->targets.emplace(newId, broadcast ? Impl::Targets{} : std::move(mergedTargets));

    return newId;
}

bool DebugDrawingHandler::shift(GeoId id, Vec3 const& v) {
    if (id.value == 0) return false;

    return impl->geoPackets.modify_if(id, [this, id, v](auto&& iter) {
        ll::ConcurrentDenseMap<
            std::pair<ChunkPos, int>,
            std::vector<std::weak_ptr<PrimitiveShapesPacket>>>
            temMap; // 用来处理shape跨区块

        std::erase_if(iter.second, [&temMap, &v](auto&& packet) {
            if (packet && !packet->mShapes->empty() && (*packet->mShapes)[0].mLocation->has_value()
                && (*packet->mShapes)[0].mDimensionId->has_value()) {
                auto chunkPos = ChunkPos((*packet->mShapes)[0].mLocation->value());
                auto dimId    = (int)(*packet->mShapes)[0].mDimensionId->value();

                // 插入空值，用于标记原区块
                temMap.try_emplace(std::make_pair(chunkPos, dimId));

                // 处理移动后的包
                for (auto& shape : *packet->mShapes) {
                    if (shape.mLocation->has_value()) {
                        shape.mLocation->value() += v;
                        if (std::holds_alternative<ArrowDataPayload>(*shape.mExtraDataPayload)) {
                            std::get<ArrowDataPayload>(*shape.mExtraDataPayload)
                                .mEndLocation->value() += v;
                        } else if (std::holds_alternative<LineDataPayload>(
                                       *shape.mExtraDataPayload
                                   )) {
                            *std::get<LineDataPayload>(*shape.mExtraDataPayload).mEndLocation += v;
                        }
                    }
                }
                auto newChunkPos    = ChunkPos((*packet->mShapes)[0].mLocation->value());
                auto [it, inserted] = temMap.try_emplace(std::make_pair(newChunkPos, dimId));
                it->second.emplace_back(std::weak_ptr<PrimitiveShapesPacket>(packet));
                return false;
            }
            return true;
        });

        // 此时，temMap中数据为空代表需要删除该区块中的对应geoId的数据；否则为增改
        for (auto& [key, data] : temMap) {
            if (data.empty()) {
                // 删
                impl->chunkPackets.erase_if(key, [&id, this](auto&& iter) {
                    auto it = std::lower_bound(
                        iter.second.begin(),
                        iter.second.end(),
                        id,
                        impl->compareByGeoId
                    );
                    if (it != iter.second.end() && it->first.value == id.value)
                        iter.second.erase(it);
                    return iter.second.empty();
                });
            }
            // 增改
            else {
                auto [it, inserted] = impl->chunkPackets.try_emplace(key);
                if (inserted) {
                    it->second.emplace_back(id, std::move(data));
                } else {
                    auto _it = std::lower_bound(
                        it->second.begin(),
                        it->second.end(),
                        id,
                        impl->compareByGeoId
                    );
                    if (_it != it->second.end() && _it->first.value == id.value)
                        _it->second = std::move(data);
                    else it->second.emplace(_it, id, std::move(data));
                }
            }
        }
        Impl::Targets target;
        if (auto it = impl->targets.find(id); it != impl->targets.end()) target = it->second;

        for (auto& pkt : iter.second)
            ll::thread::ServerThreadExecutor::getDefault().execute([pkt, target] {
                if (pkt && !pkt->mShapes->empty() && (*pkt->mShapes)[0].mLocation->has_value()
                    && (*pkt->mShapes)[0].mDimensionId->has_value()) {
                    Impl::sendPacket(
                        *pkt,
                        (*pkt->mShapes)[0].mDimensionId->value(),
                        (*pkt->mShapes)[0].mLocation->value(),
                        target
                    );
                }
            });
    });
}
} // namespace bsci