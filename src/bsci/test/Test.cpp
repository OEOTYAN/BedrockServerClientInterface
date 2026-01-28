#include "BedrockServerClientInterface.h"

#ifdef TEST
#include "bsci/GeometryGroup.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/runtime/ParamKind.h"
#include "ll/api/command/runtime/RuntimeCommand.h"
#include "ll/api/command/runtime/RuntimeOverload.h"
#include "mc/deps/core/math/Color.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/level/Level.h"
#include <ll/api/memory/Hook.h>
#include <memory>


namespace bsci::test {
void registerTestCommand(
    std::unique_ptr<GeometryGroup>&    geo,
    std::vector<GeometryGroup::GeoId>& gids
) {

    auto& cmd = ll::command::CommandRegistrar::getInstance(false)
                    .getOrCreateCommand("bsci", "bsci api test", CommandPermissionLevel::Any);

    cmd.runtimeOverload()
        .text("point")
        .required("dim", ll::command::ParamKind::Dimension)
        .required("pos", ll::command::ParamKind::Vec3)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 pos = self["pos"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            DimensionType dim = self["dim"].get<ll::command::ParamKind::Dimension>().id;
            output.success("a");
            auto gid = geo->point(dim, pos);
            output.success("a");
            gids.emplace_back(gid);
            output.success("draw point");
        });

    cmd.runtimeOverload()
        .text("line")
        .required("dim", ll::command::ParamKind::Dimension)
        .required("begin", ll::command::ParamKind::Vec3)
        .required("end", ll::command::ParamKind::Vec3)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 begin = self["begin"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            Vec3 end = self["end"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            DimensionType dim = self["dim"].get<ll::command::ParamKind::Dimension>().id;
            gids.emplace_back(geo->line(dim, begin, end));
            output.success("draw line");
        });

    cmd.runtimeOverload()
        .text("box")
        .required("dim", ll::command::ParamKind::Dimension)
        .required("begin", ll::command::ParamKind::Vec3)
        .required("end", ll::command::ParamKind::Vec3)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 begin = self["begin"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            Vec3 end = self["end"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            DimensionType dim = self["dim"].get<ll::command::ParamKind::Dimension>().id;
            gids.emplace_back(geo->box(dim, AABB(begin, end)));
            output.success("draw box");
        });

    cmd.runtimeOverload()
        .text("circle")
        .required("dim", ll::command::ParamKind::Dimension)
        .required("center", ll::command::ParamKind::Vec3)
        .required("radius", ll::command::ParamKind::Float)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 center = self["center"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            auto          radius = self["radius"].get<ll::command::ParamKind::Float>();
            DimensionType dim    = self["dim"].get<ll::command::ParamKind::Dimension>().id;
            gids.emplace_back(geo->circle(dim, center, {0, 1, 0}, radius));
            output.success("draw circle");
        });

    cmd.runtimeOverload()
        .text("circle_with_thick")
        .required("dim", ll::command::ParamKind::Dimension)
        .required("center", ll::command::ParamKind::Vec3)
        .required("radius", ll::command::ParamKind::Float)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 center = self["center"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            auto          radius = self["radius"].get<ll::command::ParamKind::Float>();
            DimensionType dim    = self["dim"].get<ll::command::ParamKind::Dimension>().id;
            gids.emplace_back(geo->circle(dim, center, {0, 1, 0}, radius, mce::Color::WHITE(), 1));
            output.success("draw circle");
        });

    cmd.runtimeOverload()
        .text("cylinder")
        .required("dim", ll::command::ParamKind::Dimension)
        .required("topCenter", ll::command::ParamKind::Vec3)
        .required("bottomCenter", ll::command::ParamKind::Vec3)
        .required("radius", ll::command::ParamKind::Float)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 topCenter = self["topCenter"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            Vec3 bottomCenter =
                self["bottomCenter"].get<ll::command::ParamKind::Vec3>().getPosition(
                    CommandVersion::CurrentVersion(),
                    origin,
                    Vec3::ZERO()
                );
            auto          radius = self["radius"].get<ll::command::ParamKind::Float>();
            DimensionType dim    = self["dim"].get<ll::command::ParamKind::Dimension>().id;
            gids.emplace_back(geo->cylinder(dim, topCenter, bottomCenter, radius));
            output.success("draw cylinder");
        });

    cmd.runtimeOverload()
        .text("sphere")
        .required("dim", ll::command::ParamKind::Dimension)
        .required("center", ll::command::ParamKind::Vec3)
        .required("radius", ll::command::ParamKind::Float)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 center = self["center"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            auto          radius = self["radius"].get<ll::command::ParamKind::Float>();
            DimensionType dim    = self["dim"].get<ll::command::ParamKind::Dimension>().id;
            gids.emplace_back(geo->sphere(dim, center, radius));
            output.success("draw sphere");
        });

    cmd.runtimeOverload().text("clear").execute(
        [&geo,
         &gids](CommandOrigin const&, CommandOutput& output, ll::command::RuntimeCommand const&) {
            for (auto& gid : gids) {
                geo->remove(gid);
            }
            output.success("clear all");
        }
    );

    cmd.runtimeOverload()
        .text("shift")
        .required("offset", ll::command::ParamKind::Vec3)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 offset = self["offset"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            for (auto& gid : gids) {
                geo->shift(gid, offset);
            }

            output.success("clear all");
        });

    cmd.runtimeOverload()
        .text("shift")
        .required("offset", ll::command::ParamKind::Vec3)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 offset = self["offset"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            for (auto& gid : gids) {
                geo->shift(gid, offset);
            }

            output.success("clear all");
        });

    cmd.runtimeOverload()
        .text("circle2")
        .required("dim", ll::command::ParamKind::Dimension)
        .required("center", ll::command::ParamKind::Vec3)
        .required("radius", ll::command::ParamKind::Float)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 center = self["center"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            auto          radius = self["radius"].get<ll::command::ParamKind::Float>();
            DimensionType dim    = self["dim"].get<ll::command::ParamKind::Dimension>().id;
            gids.emplace_back(geo->circle2(dim, center, {0, 1, 0}, radius, mce::Color::WHITE()));
            output.success("draw circle");
        });

    cmd.runtimeOverload()
        .text("sphere2")
        .required("dim", ll::command::ParamKind::Dimension)
        .required("center", ll::command::ParamKind::Vec3)
        .required("radius", ll::command::ParamKind::Float)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 center = self["center"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            auto          radius = self["radius"].get<ll::command::ParamKind::Float>();
            DimensionType dim    = self["dim"].get<ll::command::ParamKind::Dimension>().id;
            gids.emplace_back(geo->sphere2(dim, center, radius));
            output.success("draw sphere");
        });

    cmd.runtimeOverload()
        .text("arrow")
        .required("dim", ll::command::ParamKind::Dimension)
        .required("begin", ll::command::ParamKind::Vec3)
        .required("end", ll::command::ParamKind::Vec3)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 begin = self["begin"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            Vec3 end = self["end"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            DimensionType dim = self["dim"].get<ll::command::ParamKind::Dimension>().id;
            gids.emplace_back(geo->arrow(dim, begin, end));
            output.success("draw arrow");
        });

    cmd.runtimeOverload()
        .text("text")
        .required("dim", ll::command::ParamKind::Dimension)
        .required("pos", ll::command::ParamKind::Vec3)
        .required("text", ll::command::ParamKind::String)
        .execute([&geo, &gids](
                     CommandOrigin const&               origin,
                     CommandOutput&                     output,
                     ll::command::RuntimeCommand const& self
                 ) {
            Vec3 pos = self["pos"].get<ll::command::ParamKind::Vec3>().getPosition(
                CommandVersion::CurrentVersion(),
                origin,
                Vec3::ZERO()
            );
            std::string   text = self["text"].get<ll::command::ParamKind::String>();
            DimensionType dim  = self["dim"].get<ll::command::ParamKind::Dimension>().id;
            gids.emplace_back(geo->text(dim, pos + Vec3{1, 1, 1}, text, mce::Color::WHITE(), 30));
            output.success("draw text");
        });
}
} // namespace bsci::test
#endif