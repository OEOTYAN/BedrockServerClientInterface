#include "GeometryGroup.h"
#include "BedrockServerClientInterface.h"

#include <numbers>
#include <ranges>

namespace bsci {

GeometryGroup::GeoId GeometryGroup::getNextGeoId() const {
    static std::atomic_uint64_t id{};
    return {id++};
}
GeometryGroup::GeoId GeometryGroup::line(
    DimensionType        dim,
    std::span<Vec3>      dots,
    mce::Color const&    color,
    std::optional<float> thickness
) {
    std::vector<GeoId> ids;
    ids.reserve(dots.size() - 1);
    for (auto [begin, end] : dots | std::views::pairwise) {
        ids.emplace_back(line(dim, begin, end, color, thickness));
    }
    return merge(ids);
}
GeometryGroup::GeoId GeometryGroup::box(
    DimensionType        dim,
    AABB const&          box,
    mce::Color const&    color,
    std::optional<float> thickness
) {
    // clang-format off
    auto ids = std::array{
        line(dim, {box.min.x, box.min.y, box.min.z}, {box.min.x, box.min.y, box.max.z}, color, thickness),
        line(dim, {box.max.x, box.min.y, box.min.z}, {box.max.x, box.min.y, box.max.z}, color, thickness),
        line(dim, {box.min.x, box.max.y, box.min.z}, {box.min.x, box.max.y, box.max.z}, color, thickness),
        line(dim, {box.max.x, box.max.y, box.min.z}, {box.max.x, box.max.y, box.max.z}, color, thickness),

        line(dim, {box.min.x, box.min.y, box.min.z}, {box.min.x, box.max.y, box.min.z}, color, thickness),
        line(dim, {box.max.x, box.min.y, box.min.z}, {box.max.x, box.max.y, box.min.z}, color, thickness),
        line(dim, {box.min.x, box.min.y, box.max.z}, {box.min.x, box.max.y, box.max.z}, color, thickness),
        line(dim, {box.max.x, box.min.y, box.max.z}, {box.max.x, box.max.y, box.max.z}, color, thickness),

        line(dim, {box.min.x, box.min.y, box.min.z}, {box.max.x, box.min.y, box.min.z}, color, thickness),
        line(dim, {box.min.x, box.max.y, box.min.z}, {box.max.x, box.max.y, box.min.z}, color, thickness),
        line(dim, {box.min.x, box.min.y, box.max.z}, {box.max.x, box.min.y, box.max.z}, color, thickness),
        line(dim, {box.min.x, box.max.y, box.max.z}, {box.max.x, box.max.y, box.max.z}, color, thickness),
    };
    // clang-format on
    return merge(ids);
}
GeometryGroup::GeoId GeometryGroup::circle(
    DimensionType        dim,
    Vec3 const&          center,
    Vec3 const&          normal,
    float                radius,
    mce::Color const&    color,
    std::optional<float> thickness
) {
    auto const& config = BedrockServerClientInterface::getInstance().getConfig().particle;

    size_t const points = std::clamp(
        (size_t)std::ceil(radius * std::numbers::pi * 2 / config.minCircleSpacing),
        7ui64,
        config.maxCircleSegments

    );
    auto const [t, b] = branchlessONB(normal);
    auto const delta  = std::numbers::pi * 2 / (double)points;

    std::vector<GeoId> ids;
    ids.reserve(points);
    double thetaLast{0};
    for (size_t i{1}; i <= points; i++) {
        double theta = (double)i * delta;
        ids.emplace_back(line(
            dim,
            center + t * (radius * std::cos(thetaLast)) + b * radius * std::sin(thetaLast),
            center + t * (radius * std::cos(theta)) + b * radius * std::sin(theta),
            color,
            thickness
        ));
        thetaLast = theta;
    }
    return merge(ids);
}
GeometryGroup::GeoId GeometryGroup::sphere(
    DimensionType        dim,
    Vec3 const&          center,
    float                radius,
    mce::Color const&    color,
    std::optional<float> thickness
) {
    auto const& config = BedrockServerClientInterface::getInstance().getConfig().particle;

    size_t const cells = std::clamp(
        (size_t)std::ceil(radius * 2 / config.minSphereSpacing),
        2ui64,
        config.maxSphereCells
    );
    auto lines = std::vector{
        std::pair{Vec3{-1, -1, -1}, Vec3{-1, -1, +1}},
        std::pair{Vec3{+1, -1, -1}, Vec3{+1, -1, +1}},
        std::pair{Vec3{-1, +1, -1}, Vec3{-1, +1, +1}},
        std::pair{Vec3{+1, +1, -1}, Vec3{+1, +1, +1}},

        std::pair{Vec3{-1, -1, -1}, Vec3{-1, +1, -1}},
        std::pair{Vec3{+1, -1, -1}, Vec3{+1, +1, -1}},
        std::pair{Vec3{-1, -1, +1}, Vec3{-1, +1, +1}},
        std::pair{Vec3{+1, -1, +1}, Vec3{+1, +1, +1}},

        std::pair{Vec3{-1, -1, -1}, Vec3{+1, -1, -1}},
        std::pair{Vec3{-1, +1, -1}, Vec3{+1, +1, -1}},
        std::pair{Vec3{-1, -1, +1}, Vec3{+1, -1, +1}},
        std::pair{Vec3{-1, +1, +1}, Vec3{+1, +1, +1}},
    };

    for (size_t i = 1; i < cells; i++) {
        float p = ((float)i / (float)cells) * 2 - 1;
        lines.emplace_back(Vec3{p, -1, -1}, Vec3{p, -1, +1});
        lines.emplace_back(Vec3{p, +1, -1}, Vec3{p, +1, +1});
        lines.emplace_back(Vec3{p, -1, -1}, Vec3{p, +1, -1});
        lines.emplace_back(Vec3{p, -1, +1}, Vec3{p, +1, +1});

        lines.emplace_back(Vec3{-1, p, -1}, Vec3{-1, p, +1});
        lines.emplace_back(Vec3{+1, p, -1}, Vec3{+1, p, +1});
        lines.emplace_back(Vec3{-1, p, -1}, Vec3{+1, p, -1});
        lines.emplace_back(Vec3{-1, p, +1}, Vec3{+1, p, +1});

        lines.emplace_back(Vec3{-1, -1, p}, Vec3{-1, +1, p});
        lines.emplace_back(Vec3{+1, -1, p}, Vec3{+1, +1, p});
        lines.emplace_back(Vec3{-1, -1, p}, Vec3{+1, -1, p});
        lines.emplace_back(Vec3{-1, +1, p}, Vec3{+1, +1, p});
    }
    std::vector<GeoId> ids;
    ids.reserve(lines.size() * cells);

    for (auto const& [begin, end] : lines) {
        Vec3 lastPos = center + begin.normalize() * radius;
        for (size_t i = 1; i <= cells; i++) {
            Vec3 pos = lerp(begin, end, {(float)i / (float)cells});
            pos      = center + pos.normalize() * radius;
            ids.emplace_back(line(dim, lastPos, pos, color, thickness));
            lastPos = pos;
        }
    }
    return merge(ids);
}
} // namespace bsci
