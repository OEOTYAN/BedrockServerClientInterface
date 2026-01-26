#include "DebugDrawingPacketHandler.h"
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
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <parallel_hashmap/phmap.h>
#include <string>
#include <utility>
#include <variant>
#include <vector>


TextDataPayload::TextDataPayload() = default;
TextDataPayload::TextDataPayload(TextDataPayload const& cp) { mText = cp.mText; };
// ShapeDataPayload::ShapeDataPayload() = default;
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
std::unique_ptr<GeometryGroup> GeometryGroup::createDefault() {
    return std::make_unique<DebugDrawingPacketHandler>();
}

static std::atomic<uint64_t> nextId_{UINT64_MAX};

class DebugDrawingPacketHandler::Impl {
public:
    using PacketPair = std::pair<GeoId, std::vector<std::weak_ptr<DebugDrawerPacket>>>;
    struct Hook;
    size_t                           id{};
    std::unique_ptr<ParticleSpawner> particleSpawner =
        std::make_unique<ParticleSpawner>(); // 向前兼容
    ph_flat_hash_map<GeoId, std::pair<std::vector<std::shared_ptr<DebugDrawerPacket>>, bool>>
        geoPackets; // bool = true 表示启用ParticleSpawner
    ph_flat_hash_map<std::pair<ChunkPos, int>, std::vector<PacketPair>>
        chunkPackets; // 不应包含使用ParticleSpawner的GeoId

public:
    // void sendPacketImmediately(DebugDrawerPacket& pkt) {
    //     if (pkt.mShapes->empty()) return;
    //     ll::thread::ServerThreadExecutor::getDefault().execute([pkt] { pkt.sendToClients(); });
    // }

    static bool compareByGeoId(const PacketPair& a, const GeoId& b) {
        return a.first.value < b.value;
    }

    void addPacket(
        GeometryGroup::GeoId                geoId,
        std::shared_ptr<DebugDrawerPacket>& packet,
        int                                 dimId,
        Vec3 const&                         pos
    ) {
        if (packet) {
            this->geoPackets.emplace(
                geoId,
                std::make_pair(std::vector<std::shared_ptr<DebugDrawerPacket>>{packet}, false)
            );
            auto chunkPos         = ChunkPos(pos);
            auto key              = std::make_pair(chunkPos, dimId);
            auto [iter, inserted] = this->chunkPackets.try_emplace(key);

            if (inserted) {
                iter->second.emplace_back(
                    geoId,
                    std::vector<std::weak_ptr<DebugDrawerPacket>>{packet}
                );
            } else {
                auto it = std::lower_bound(
                    iter->second.begin(),
                    iter->second.end(),
                    geoId,
                    compareByGeoId
                );
                iter->second
                    .emplace(it, geoId, std::vector<std::weak_ptr<DebugDrawerPacket>>{packet});
            }
        }
        // else {
        //     this->geoGroup.emplace(
        //         geoId,
        //         std::make_pair(std::vector<std::shared_ptr<DebugDrawerPacket>>{}, true)
        //     );
        // }
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
    if (packet.getId() == MinecraftPacketIds::FullChunkData && hasInstance) [[unlikely]] {

        const auto& levelChunkPacket = static_cast<LevelChunkPacket const&>(packet);
        const auto& chunkPos         = levelChunkPacket.mPos;
        const auto& dimId            = (int)*levelChunkPacket.mDimensionId;
        auto        key              = std::make_pair(chunkPos, dimId);

        std::vector<std::shared_ptr<DebugDrawerPacket>> _chunkPackets;

        {
            std::lock_guard l{listMutex};

            for (auto s : list) {
                s->impl->chunkPackets.erase_if(key, [&_chunkPackets](auto&& iter) {
                    std::erase_if(iter.second, [&_chunkPackets](auto&& packetPair) {
                        _chunkPackets.reserve(_chunkPackets.size() + packetPair.second.size());
                        std::erase_if(packetPair.second, [&_chunkPackets](auto&& pkt) {
                            if (auto shared = pkt.lock()) {
                                _chunkPackets.emplace_back(shared);
                                return false;
                            }
                            return true;
                        });
                        return packetPair.second.empty();
                    });
                    return iter.second.empty();
                });
            }
        }
        for (auto& pkt : _chunkPackets) {
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
    this->impl->geoPackets.emplace(
        geoId,
        std::make_pair(std::vector<std::shared_ptr<DebugDrawerPacket>>{}, true)
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
    if (begin == end) return {0};
    if (thickness.has_value()) {
        auto geoId = this->impl->particleSpawner->line(dim, begin, end, color, thickness);
        this->impl->geoPackets.emplace(
            geoId,
            std::make_pair(std::vector<std::shared_ptr<DebugDrawerPacket>>{}, true)
        );
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
        ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin, dim] {
            packet->sendTo(begin, dim);
        });

        auto id = GeometryGroup::getNextGeoId();
        this->impl->addPacket(id, packet, dim, begin);
        return id;
    }

    int segmentNum   = ((int)len) / 48 + 1;
    offset          /= segmentNum;
    Vec3 currentPos  = begin;
    Vec3 lastPos     = begin;

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

GeometryGroup::GeoId DebugDrawingPacketHandler::box(
    DimensionType        dim,
    AABB const&          box,
    mce::Color const&    color,
    std::optional<float> thickness
) {

    if (thickness.has_value()) {
        auto geoId = this->impl->particleSpawner->box(dim, box, color, thickness);
        this->impl->geoPackets.emplace(
            geoId,
            std::make_pair(std::vector<std::shared_ptr<DebugDrawerPacket>>{}, true)
        );
        return geoId;
    }
    if ((box.max - box.min).lengthSqr() >= 2304) return GeometryGroup::box(dim, box, color);

    auto packet = std::make_shared<DebugDrawerPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    ShapeDataPayload shape;
    shape.mNetworkId        = nextId_.fetch_sub(1);
    shape.mShapeType        = ScriptModuleDebugUtilities::ScriptDebugShapeType::Box;
    shape.mLocation         = box.min;
    shape.mColor            = color;
    shape.mDimensionId      = dim;
    shape.mExtraDataPayload = BoxDataPayload{.mBoxBound = box.max - box.min};
    packet->mShapes->emplace_back(std::move(shape));
    ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin = box.min, dim] {
        packet->sendTo(begin, dim);
    });

    auto id = GeometryGroup::getNextGeoId();
    this->impl->addPacket(id, packet, dim, box.min);
    return id;
}


GeometryGroup::GeoId DebugDrawingPacketHandler::circle2(
    DimensionType     dim,
    Vec3 const&       center,
    Vec3 const&       normal,
    float             radius,
    mce::Color const& color
) {
    if (radius > 48) return this->circle(dim, center, normal, radius, color);

    auto packet = std::make_shared<DebugDrawerPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    ShapeDataPayload shape;
    shape.mNetworkId   = nextId_.fetch_sub(1);
    shape.mShapeType   = ScriptModuleDebugUtilities::ScriptDebugShapeType::Circle;
    shape.mRotation    = normal;
    shape.mLocation    = center;
    shape.mScale       = radius;
    shape.mColor       = color;
    shape.mDimensionId = dim;
    packet->mShapes->emplace_back(std::move(shape));
    ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin = center, dim] {
        packet->sendTo(begin, dim);
    });

    auto id = GeometryGroup::getNextGeoId();
    this->impl->addPacket(id, packet, dim, center);
    return id;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::sphere2(
    DimensionType        dim,
    Vec3 const&          center,
    float                radius,
    mce::Color const&    color,
    std::optional<uchar> mNumSegments
) {
    if (radius > 48) return this->sphere(dim, center, radius, color);

    auto packet = std::make_shared<DebugDrawerPacket>();
    packet->setSerializationMode(SerializationMode::CerealOnly);
    ShapeDataPayload shape;
    shape.mNetworkId   = nextId_.fetch_sub(1);
    shape.mShapeType   = ScriptModuleDebugUtilities::ScriptDebugShapeType::Sphere;
    shape.mLocation    = center;
    shape.mScale       = radius;
    shape.mColor       = color;
    shape.mDimensionId = dim;
    if (mNumSegments.has_value()) {
        shape.mExtraDataPayload = SphereDataPayload{.mNumSegments = mNumSegments.value()};
    }
    packet->mShapes->emplace_back(std::move(shape));
    ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin = center, dim] {
        packet->sendTo(begin, dim);
    });

    auto id = GeometryGroup::getNextGeoId();
    this->impl->addPacket(id, packet, dim, center);
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

    Vec3   offset = end - begin;
    double len    = offset.length();
    if (len <= 48) {
        auto packet = std::make_shared<DebugDrawerPacket>();
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
        ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin, dim] {
            packet->sendTo(begin, dim);
        });

        auto id = GeometryGroup::getNextGeoId();
        this->impl->addPacket(id, packet, dim, begin);
        return id;
    }

    int segmentNum                 = ((int)len) / 48 + 1;
    offset                        /= segmentNum;
    Vec3               currentPos  = begin;
    Vec3               lastPos     = begin;
    std::vector<GeoId> ids;
    ids.reserve(segmentNum);
    for (int i = 0; i < segmentNum - 1; i++) {
        currentPos += offset;
        ids.emplace_back(this->line(dim, lastPos, currentPos, color));
        lastPos = currentPos;
    }
    ids.emplace_back(
        this->arrow(dim, currentPos, end, color, mArrowHeadLength, mArrowHeadRadius, mNumSegments)
    );
    return this->merge(ids);
}

GeometryGroup::GeoId DebugDrawingPacketHandler::text(
    DimensionType        dim,
    Vec3 const&          pos,
    std::string&         text,
    mce::Color const&    color,
    std::optional<float> scale
) {
    auto packet = std::make_shared<DebugDrawerPacket>();
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
    ll::thread::ServerThreadExecutor::getDefault().execute([packet, begin = pos, dim] {
        packet->sendTo(begin, dim);
    });

    auto id = GeometryGroup::getNextGeoId();
    this->impl->addPacket(id, packet, dim, pos);
    return id;
}

bool DebugDrawingPacketHandler::remove(GeoId id) {
    if (id.value == 0) {
        return false;
    }
    std::vector<std::shared_ptr<DebugDrawerPacket>> removePackets;
    this->impl->geoPackets.erase_if(id, [this, id, &removePackets](auto&& iter) {
        if (iter.second.second) this->impl->particleSpawner->remove(iter.first);
        for (auto& packet : iter.second.first) {
            if (packet && !packet->mShapes->empty()
                && (*packet->mShapes)[0].mLocation->has_value()) {
                auto chunkPos = ChunkPos((*packet->mShapes)[0].mLocation->value());
                auto dimId    = (int)*(*packet->mShapes)[0].mDimensionId;
                auto key      = std::make_pair(chunkPos, dimId);
                this->impl->chunkPackets.erase_if(key, [this, id](auto&& iter) {
                    auto it = std::lower_bound(
                        iter.second.begin(),
                        iter.second.end(),
                        id,
                        this->impl->compareByGeoId
                    );
                    if (it != iter.second.end() && it->first.value == id.value) {
                        iter.second.erase(it);
                    }
                    return iter.second.empty();
                });
            }
        }
        removePackets = std::move(iter.second.first);
        return true;
    });
    for (auto& packet : removePackets) {
        ll::thread::ServerThreadExecutor::getDefault().execute([packet] {
            if (packet) {
                for (auto& shape : *packet->mShapes) shape.mShapeType = std::nullopt;
                packet->sendToClients();
            }
        });
    }
    return true;
}

GeometryGroup::GeoId DebugDrawingPacketHandler::merge(std::span<GeoId> ids) {
    if (ids.empty()) {
        return {0};
    }
    ph_flat_hash_map<std::pair<ChunkPos, int>,
                     std::vector<GeoId>>            temMap;      // 用来处理包的合并
    std::vector<GeoId>                              particleIds; // 需要使用ParticleSpawner的id
    std::vector<std::shared_ptr<DebugDrawerPacket>> newPackets;  // 合并后的包队列

    // 移出旧id的geoPackets
    for (auto& id : ids) {
        this->impl->geoPackets.erase_if(id, [id, &temMap, &particleIds, &newPackets](auto&& iter) {
            // 处理需要使用ParticleSpawner的id
            if (iter.second.second) particleIds.emplace_back(id);

            // 处理包的合并
            std::erase_if(iter.second.first, [&temMap, id](auto&& packet) {
                if (packet && !packet->mShapes->empty()
                    && (*packet->mShapes)[0].mLocation->has_value()) {
                    auto chunkPos         = ChunkPos((*packet->mShapes)[0].mLocation->value());
                    auto dimId            = (int)*(*packet->mShapes)[0].mDimensionId;
                    auto key              = std::make_pair(chunkPos, dimId);
                    auto [iter, inserted] = temMap.try_emplace(key);

                    // 记录对应区块待合并的GeoId
                    iter->second.emplace_back(id);
                    return false;
                } else return true;
            });
            newPackets.insert(
                newPackets.end(),
                std::make_move_iterator(iter.second.first.begin()),
                std::make_move_iterator(iter.second.first.end())
            );
            return true;
        });
    }

    // 生成新id
    GeoId newId       = this->impl->particleSpawner->merge(particleIds);
    bool  useParticle = newId.value != 0;
    if (!useParticle) {
        if (newPackets.size() == 0) return {0};
        newId = GeometryGroup::getNextGeoId();
    }

    // 处理chunkPackets
    for (auto& [key, data] : temMap) {
        this->impl->chunkPackets.modify_if(key, [this, &data, &newId](auto&& iter) {
            // 合并包
            std::vector<std::weak_ptr<DebugDrawerPacket>> pkts;
            for (auto& id : data) {
                auto it = std::lower_bound(
                    iter.second.begin(),
                    iter.second.end(),
                    id,
                    this->impl->compareByGeoId
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
            std::pair<GeoId, std::vector<std::weak_ptr<DebugDrawerPacket>>> newPair =
                std::make_pair(newId, std::move(pkts));
            auto it = std::lower_bound(
                iter.second.begin(),
                iter.second.end(),
                newId,
                this->impl->compareByGeoId
            );
            iter.second.insert(it, std::move(newPair));
        });
    }

    // 添加新id的geoPackets
    this->impl->geoPackets.emplace(newId, std::make_pair(std::move(newPackets), useParticle));

    return newId;
}

bool DebugDrawingPacketHandler::shift(GeoId id, Vec3 const& v) {
    if (id.value == 0) return false;

    return this->impl->geoPackets.modify_if(id, [this, id, v](auto&& iter) {
        if (iter.second.second) this->impl->particleSpawner->shift(id, v);

        ph_flat_hash_map<
            std::pair<ChunkPos, int>,
            std::vector<std::weak_ptr<DebugDrawerPacket>>>
            temMap; // 用来处理shape跨区块

        std::erase_if(iter.second.first, [&temMap, &v](auto&& packet) {
            if (packet && !packet->mShapes->empty()
                && (*packet->mShapes)[0].mLocation->has_value()) {
                auto chunkPos = ChunkPos((*packet->mShapes)[0].mLocation->value());
                auto dimId    = (int)*(*packet->mShapes)[0].mDimensionId;

                // 插入空值，用于标记原区块
                temMap.try_emplace(std::make_pair(chunkPos, dimId));

                // 处理移动后的包
                for (auto& shape : *packet->mShapes) {
                    if (shape.mLocation->has_value()) {
                        shape.mLocation->value() += v;
                        if (std::holds_alternative<ArrowDataPayload>(*shape.mExtraDataPayload)) {
                            std::get<ArrowDataPayload>(*shape.mExtraDataPayload)
                                .mEndLocation->value() += v;
                        } else if (std::holds_alternative<LineDataPayload>(*shape.mExtraDataPayload
                                   )) {
                            *std::get<LineDataPayload>(*shape.mExtraDataPayload).mEndLocation += v;
                        }
                    }
                }
                auto newChunkPos    = ChunkPos((*packet->mShapes)[0].mLocation->value());
                auto [it, inserted] = temMap.try_emplace(std::make_pair(newChunkPos, dimId));
                it->second.emplace_back(std::weak_ptr<DebugDrawerPacket>(packet));
                return false;
            }
            return true;
        });

        // 此时，temMap中数据为空代表需要删除该区块中的对应geoId的数据；否则为增改
        for (auto& [key, data] : temMap) {
            if (data.empty()) {
                // 删
                this->impl->chunkPackets.erase_if(key, [&id, this](auto&& iter) {
                    auto it = std::lower_bound(
                        iter.second.begin(),
                        iter.second.end(),
                        id,
                        this->impl->compareByGeoId
                    );
                    if (it != iter.second.end() && it->first.value == id.value)
                        iter.second.erase(it);
                    return iter.second.empty();
                });
            }
            // 增改
            else {
                auto [it, inserted] = this->impl->chunkPackets.try_emplace(key);
                if (inserted) {
                    it->second.emplace_back(id, std::move(data));
                } else {
                    auto _it = std::lower_bound(
                        it->second.begin(),
                        it->second.end(),
                        id,
                        this->impl->compareByGeoId
                    );
                    if (_it != it->second.end() && _it->first.value == id.value)
                        _it->second = std::move(data);
                    else it->second.emplace(_it, id, std::move(data));
                }
            }
        }
        for (auto& pkt : iter.second.first)
            ll::thread::ServerThreadExecutor::getDefault().execute([pkt] {
                if (pkt && !pkt->mShapes->empty() && (*pkt->mShapes)[0].mLocation->has_value()) {
                    pkt->sendTo(
                        (*pkt->mShapes)[0].mLocation->value(),
                        (*pkt->mShapes)[0].mDimensionId
                    );
                }
            });
    });
}
} // namespace bsci