#include "particle/ParticleSender.h"

#include <ll/api/service/Bedrock.h>
#include <mc/network/packet/SpawnParticleEffectPacket.h>
#include <mc/util/molang/MolangScriptArg.h>
#include <mc/util/molang/MolangVariableSettings.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/dimension/Dimension.h>

namespace bsci {
struct ParticleSender::Impl {
    float                        lifeTime;
    float                        lineThickness;
    std::optional<ActorUniqueID> actor;
};

ParticleSender::ParticleSender(
    float                        lifeTime,
    float                        lineThickness,
    std::optional<ActorUniqueID> actorId
)
: impl(std::make_unique<Impl>(lifeTime, lineThickness, actorId)) {}
ParticleSender::~ParticleSender() = default;


void ParticleSender::particle(
    std::string const& name,
    Vec3 const&        pos,
    DimensionType      dim,
    MolangVariableMap  var
) const {
    if (auto dimptr = ll::service::getLevel()->getDimension(dim)) {
        var.setMolangVariable("variable.bsci_particle_lifetime", impl->lifeTime);
        auto packet = SpawnParticleEffectPacket(pos, name, dim, std::move(var));
        if (impl->actor) {
            packet.mActorId = *impl->actor;
        }
        dimptr->sendPacketForPosition(pos, packet, nullptr);
    }
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
        MolangMemberArray{MolangStruct_RGB{}, tint}
    );
}
static void addDirection(MolangVariableMap& var, Vec3 const& dir) {
    var.setMolangVariable(
        "variable.bsci_particle_direction",
        MolangMemberArray{MolangStruct_XYZ{}, dir}
    );
}

void ParticleSender::line(
    DimensionType     dim,
    Vec3 const&       begin,
    Vec3 const&       end,
    mce::Color const& color
) const {
    MolangVariableMap var;
    addSize(var, {begin.distanceTo(end), impl->lineThickness});
    addDirection(var, (end - begin).normalize());
    addTint(var, color);
    particle("ll:line", (begin + end) * 0.5f, dim, std::move(var));
}
void ParticleSender::box(DimensionType dim, AABB const& box, mce::Color const& color) const {
    line(dim, {box.min.x, box.min.y, box.min.z}, {box.min.x, box.min.y, box.max.z}, color);
    line(dim, {box.max.x, box.min.y, box.min.z}, {box.max.x, box.min.y, box.max.z}, color);
    line(dim, {box.min.x, box.max.y, box.min.z}, {box.min.x, box.max.y, box.max.z}, color);
    line(dim, {box.max.x, box.max.y, box.min.z}, {box.max.x, box.max.y, box.max.z}, color);

    line(dim, {box.min.x, box.min.y, box.min.z}, {box.min.x, box.max.y, box.min.z}, color);
    line(dim, {box.max.x, box.min.y, box.min.z}, {box.max.x, box.max.y, box.min.z}, color);
    line(dim, {box.min.x, box.min.y, box.max.z}, {box.min.x, box.max.y, box.max.z}, color);
    line(dim, {box.max.x, box.min.y, box.max.z}, {box.max.x, box.max.y, box.max.z}, color);

    line(dim, {box.min.x, box.min.y, box.min.z}, {box.max.x, box.min.y, box.min.z}, color);
    line(dim, {box.min.x, box.max.y, box.min.z}, {box.max.x, box.max.y, box.min.z}, color);
    line(dim, {box.min.x, box.min.y, box.max.z}, {box.max.x, box.min.y, box.max.z}, color);
    line(dim, {box.min.x, box.max.y, box.max.z}, {box.max.x, box.max.y, box.max.z}, color);
}

} // namespace bsci