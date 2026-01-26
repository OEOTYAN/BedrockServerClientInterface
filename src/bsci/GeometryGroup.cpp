#include "GeometryGroup.h"
#include "BedrockServerClientInterface.h"
#include "bsci/utils/Math.h"

#include <numbers>
#include <ranges>

namespace bsci {

GeometryGroup::GeoId GeometryGroup::getNextGeoId() const {
    static std::atomic_uint64_t id{};
    return {++id};
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
    Vec3 lastPos = t * radius;
    for (size_t i{1}; i <= points; i++) {
        double theta = (double)i * delta;
        Vec3   pos   = t * (radius * std::cos(theta)) + b * radius * std::sin(theta);
        ids.emplace_back(line(dim, center + lastPos, center + pos, color, thickness));
        lastPos = pos;
    }
    return merge(ids);
}
GeometryGroup::GeoId GeometryGroup::cylinder(
    DimensionType        dim,
    Vec3 const&          topCenter,
    Vec3 const&          bottomCenter,
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
    auto const [t, b] = branchlessONB((topCenter - bottomCenter).normalize());
    auto const delta  = std::numbers::pi * 2 / (double)points;

    std::vector<GeoId> ids;
    ids.reserve(points);
    Vec3 lastPos = t * radius;
    for (size_t i{1}; i <= points; i++) {
        double theta = (double)i * delta;
        Vec3   pos   = t * (radius * std::cos(theta)) + b * radius * std::sin(theta);
        ids.emplace_back(line(dim, topCenter + lastPos, topCenter + pos, color, thickness));
        ids.emplace_back(line(dim, topCenter + pos, bottomCenter + pos, color, thickness));
        ids.emplace_back(line(dim, bottomCenter + lastPos, bottomCenter + pos, color, thickness));
        lastPos = pos;
    }
    return merge(ids);
}

static Vec3 cubeToSphere(Vec3 const& v) {
    auto v2 = v * v;
    return v
         * sqrt(Vec3{
             1 - (v2.y + v2.z) / 2 + (v2.y * v2.z) / 3,
             1 - (v2.z + v2.x) / 2 + (v2.z * v2.x) / 3,
             1 - (v2.x + v2.y) / 2 + (v2.x * v2.y) / 3
         });
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
        Vec3 lastPos = center + cubeToSphere(begin) * radius;
        for (size_t i = 1; i <= cells; i++) {
            Vec3 pos = lerp(begin, end, {(float)i / (float)cells});
            pos      = center + cubeToSphere(pos) * radius;
            ids.emplace_back(line(dim, lastPos, pos, color, thickness));
            lastPos = pos;
        }
    }
    return merge(ids);
}

GeometryGroup::GeoId GeometryGroup::circle2(
    DimensionType     dim,
    Vec3 const&       center,
    Vec3 const&       normal,
    float             radius,
    mce::Color const& color
) {
    return circle(dim, center, normal, radius, color);
}

GeometryGroup::GeoId GeometryGroup::
    sphere2(DimensionType dim, Vec3 const& center, float radius, mce::Color const& color, std::optional<uchar>) {
    return sphere(dim, center, radius, color);
}

GeometryGroup::GeoId GeometryGroup::
    arrow(DimensionType dim, Vec3 const& begin, Vec3 const& end, mce::Color const& color, std::optional<float>, std::optional<float>, std::optional<uchar>) {
    return line(dim, begin, end, color);
}

GeometryGroup::GeoId GeometryGroup::
    text(DimensionType, Vec3 const&, std::string&, mce::Color const&, std::optional<float>) {
    return {};
}
} // namespace bsci
