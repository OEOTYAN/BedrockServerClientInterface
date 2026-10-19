#include "bsci/particle/ParticleSpawner.h"
#include "BedrockServerClientInterface.h"
#include "bsci/utils/Utils.h"

#include <ll/api/base/Containers.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/Listener.h>
#include <ll/api/event/world/ServerLevelTickEvent.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/service/GamingStatus.h>
#include <ll/api/thread/ServerThreadExecutor.h>

#include <atomic>
#include <mc/deps/core/string/HashedString.h>
#include <mc/deps/core/threading/Threading.h>
#include <mc/legacy/ActorUniqueID.h>
#include <mc/network/packet/SpawnParticleEffectPacket.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/level/Level.h>
#include <mc/platform/threading/AssignedThread.h>
#include <mc/util/MolangMemberArray.h>
#include <mc/util/MolangMemberVariable.h>
#include <mc/util/MolangScriptArg.h>
#include <mc/util/MolangStruct_RGBA.h>
#include <mc/util/MolangStruct_XY.h>
#include <mc/util/MolangStruct_XYZ.h>
#include <mc/util/MolangVariable.h>
#include <mc/util/MolangVariableMap.h>
#include <mc/util/MolangVariableSettings.h>
#include <mc/util/Timer.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/dimension/Dimension.h>


// MolangMemberArray::MolangMemberArray() = default;
MolangVariableMap::MolangVariableMap() = default;
// MolangScriptArg::MolangScriptArg() = default;
// MolangVariableMap::MolangVariableMap(MolangVariableMap const& rhs) {
//     mMapFromVariableIndexToVariableArrayOffset =
//     rhs.mMapFromVariableIndexToVariableArrayOffset; mVariables = {}; for (auto& ptr :
//     *rhs.mVariables) {
//         mVariables->push_back(std::make_unique<MolangVariable>(*ptr));
//     }
//     mHasPublicVariables = rhs.mHasPublicVariables;
// }

namespace bsci {
// std::unique_ptr<GeometryGroup> GeometryGroup::createDefault() {
//     return std::make_unique<ParticleSpawner>();
// }

static void addTime(MolangVariableMap& var) {
    auto& config = BedrockServerClientInterface::getInstance().getConfig().particle;
    var.setMolangVariable(
        "variable.bsci_particle_lifetime",
        (float)(config.extraTime + ((64.0 / 20.0) / (double)config.tablePerTick))
    );
}
static void addSize(MolangVariableMap& var, Vec2 const& size) {
    var.setMolangVariable(
        "variable.bsci_particle_size",
        MolangMemberArray{MolangStruct_XY{}, size * 0.5f}
    );
}
static void addTint(MolangVariableMap& var, mce::Color const& tint) {
    var.setMolangVariable(
        "variable.bsci_particle_tint",
        MolangMemberArray{MolangStruct_RGBA{}, tint}
    );
}
static void addDirection(MolangVariableMap& var, Vec3 const& dir) {
    var.setMolangVariable(
        "variable.bsci_particle_direction",
        MolangMemberArray{MolangStruct_XYZ{}, dir}
    );
}
struct ParticleSpawner::Impl {
    struct Entry {
        std::unique_ptr<SpawnParticleEffectPacket> packet;
        std::vector<ActorUniqueID>                 players;
    };

    std::atomic_bool       active{true};
    size_t                 tickId{};
    ll::event::ListenerPtr listener;
    size_t                 id{};
    ll::ConcurrentDenseMap<
        GeoId,
        Entry,
        ::phmap::priv::hash_default_hash<GeoId>,
        ::phmap::priv::hash_default_eq<GeoId>,
        ::std::allocator<::std::pair<GeoId const, Entry>>,
        6>
                                                      geoPackets;
    ll::ConcurrentDenseMap<GeoId, std::vector<GeoId>> geoGroup;

    static void
    sendPacket(SpawnParticleEffectPacket const& packet, std::vector<ActorUniqueID> const& players) {
        if (players.empty()) {
            packet.sendTo(*packet.mPos, packet.mVanillaDimensionId);
            return;
        }

        auto level = ll::service::getLevel();
        if (!level) return;
        auto dimension = level->getDimension(packet.mVanillaDimensionId).lock();
        if (!dimension) return;

        std::vector<Player*> targets;
        targets.reserve(players.size());
        for (auto const& id : players) {
            if (auto* player = level->getPlayer(id)) targets.emplace_back(player);
        }
        if (targets.empty()) return;
        sendToPlayerAt(*dimension, *packet.mPos, packet, targets);
    }

    void sendParticleImmediately(
        SpawnParticleEffectPacket&        packet,
        std::vector<ActorUniqueID> const& players
    ) {
        if (BedrockServerClientInterface::getInstance().getConfig().particle.delayUndate) {
            return;
        }
        ll::thread::ServerThreadExecutor::getDefault().execute([packet, players] {
            Impl::sendPacket(packet, players);
        });
    }

    void tick() {
        if (!active.load(std::memory_order_acquire)) {
            return;
        }
        auto const tablePerTick =
            BedrockServerClientInterface::getInstance().getConfig().particle.tablePerTick;
        auto begin = (tickId % (64 / tablePerTick)) * tablePerTick;
        for (size_t i = 0; i < tablePerTick; i++) {
            geoPackets.with_submap(begin + i, [&](auto& map) {
                for (auto& [id, entry] : map) {
                    if (entry.packet) {
                        Impl::sendPacket(*entry.packet, entry.players);
                    }
                }
            });
        }
        ++tickId;
    }
};

ParticleSpawner::ParticleSpawner() : impl(std::make_shared<Impl>()) {
    impl->listener =
        ll::event::EventBus::getInstance().emplaceListener<ll::event::world::ServerLevelTickEvent>(
            [impl = impl](ll::event::world::ServerLevelTickEvent&) { impl->tick(); }
        );
}
ParticleSpawner::~ParticleSpawner() {
    if (impl) {
        impl->active.store(false, std::memory_order_release);
        if (impl->listener) {
            ll::event::EventBus::getInstance()
                .removeListener<ll::event::world::ServerLevelTickEvent>(impl->listener);
            impl->listener.reset();
        }
    }
}

GeometryGroup::GeoId ParticleSpawner::particle(
    DimensionType               dim,
    Vec3 const&                 pos,
    std::string const&          name,
    MolangVariableMap           var,
    std::vector<Player*> const& players
) const {
    addTime(var);
    auto packet =
        std::make_unique<SpawnParticleEffectPacket>(pos, name, (uchar)dim, std::move(var));

    std::vector<ActorUniqueID> targets;
    targets.reserve(players.size());
    for (auto* player : players) {
        if (player) targets.emplace_back(player->getOrCreateUniqueID());
    }

    impl->sendParticleImmediately(*packet, targets);
    auto id = GeometryGroup::getNextGeoId();
    impl->geoPackets.try_emplace(id, Impl::Entry{std::move(packet), std::move(targets)});
    return id;
}

GeometryGroup::GeoId ParticleSpawner::line(
    DimensionType               dim,
    Vec3 const&                 begin,
    Vec3 const&                 end,
    mce::Color const&           color,
    std::optional<float>        thickness,
    std::vector<Player*> const& players
) {
    if (begin == end) return GeoId::invalid();
    MolangVariableMap var;
    addSize(
        var,
        {begin.distanceTo(end),
         thickness.value_or(
             BedrockServerClientInterface::getInstance().getConfig().particle.defaultThickness
         )}
    );
    addDirection(var, (end - begin).normalize());
    addTint(var, color);
    return particle(
        dim,
        (begin + end) * 0.5f,
        color.a == 1 ? "bsci:line" : "bsci:blend_line",
        std::move(var),
        players
    );
}

GeometryGroup::GeoId ParticleSpawner::point(
    DimensionType               dim,
    Vec3 const&                 pos,
    mce::Color const&           color,
    std::optional<float>        radius,
    std::vector<Player*> const& players
) {
    MolangVariableMap var;
    addSize(
        var,
        {radius.value_or(
            BedrockServerClientInterface::getInstance().getConfig().particle.defaultPointRadius
        )}
    );
    addTint(var, color);
    return particle(
        dim,
        pos,
        color.a == 1 ? "bsci:point" : "bsci:blend_point",
        std::move(var),
        players
    );
}

bool ParticleSpawner::remove(GeoId id) {
    if (id.value == 0) {
        return false;
    }
    if (!impl->geoGroup.erase_if(id, [this](auto&& iter) {
            for (auto& subId : iter.second) {
                impl->geoPackets.erase(subId);
            }
            return true;
        })) {
        return impl->geoPackets.erase(id);
    }
    return true;
}

GeometryGroup::GeoId ParticleSpawner::merge(std::span<GeoId> ids) {
    if (ids.empty()) {
        return GeoId::invalid();
    }
    auto               id = GeometryGroup::getNextGeoId();
    std::vector<GeoId> res;
    res.reserve(ids.size());
    for (auto const& sid : ids) {
        if (!impl->geoGroup.erase_if(sid, [&res](auto&& iter) {
                auto& source = iter.second;
                res.reserve(res.size() + source.size());
                res.insert(
                    res.end(),
                    std::make_move_iterator(source.begin()),
                    std::make_move_iterator(source.end())
                );
                return true;
            })) {
            res.push_back(sid);
        }
    }
    impl->geoGroup.try_emplace(id, std::move(res));
    return id;
}

bool ParticleSpawner::shift(GeoId id, Vec3 const& v) {
    if (!impl->geoGroup.modify_if(id, [this, &v](auto&& i) {
            for (auto& subId : i.second) {
                impl->geoPackets.modify_if(subId, [this, &v](auto&& iter) {
                    if (iter.second.packet) {
                        *iter.second.packet->mPos += v;
                        impl->sendParticleImmediately(*iter.second.packet, iter.second.players);
                    }
                });
            }
        })) {
        return impl->geoPackets.modify_if(id, [this, &v](auto&& iter) {
            if (iter.second.packet) {
                *iter.second.packet->mPos += v;
                impl->sendParticleImmediately(*iter.second.packet, iter.second.players);
            }
        });
    }
    return true;
}

} // namespace bsci
