#pragma once

#include "bsci/Marcos.h"
#include "mc/world/phys/AABB.h"

#include <mc/deps/core/math/Color.h>
#include <mc/platform/UUID.h>
#include <mc/world/level/dimension/DimensionType.h>

#include <optional>

class Packet;
class NetworkIdentifier;

namespace bsci {
class GeometryGroup {
public:
    struct GeoId {
        uint64             value;
        constexpr bool     operator==(GeoId const& other) const { return other.value == value; }
        explicit constexpr operator bool() const { return value == 0; }

        static constexpr GeoId invalid() { return GeoId{0}; }
    };

protected:
    BSCI_API GeoId getNextGeoId() const;

    // 若设置了 viewer，则该 GeometryGroup 产生的所有图形仅发送给该玩家；否则沿用旧行为（发送给附近的所有玩家）
    std::optional<mce::UUID> mViewer;

public:
    BSCI_API static std::unique_ptr<GeometryGroup>
    createDefault(std::optional<mce::UUID> viewer = std::nullopt);

    [[nodiscard]] std::optional<mce::UUID> const& getViewer() const { return mViewer; }

    BSCI_API virtual ~GeometryGroup() = default;

    BSCI_API virtual GeoId point(
        DimensionType        dim,
        Vec3 const&          pos,
        mce::Color const&    color  = mce::Color::WHITE(),
        std::optional<float> radius = {}
    );

    virtual GeoId line(
        DimensionType        dim,
        Vec3 const&          begin,
        Vec3 const&          end,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    ) = 0;

    virtual bool remove(GeoId) = 0;

    virtual GeoId merge(std::span<GeoId>) = 0;

    virtual bool shift(GeoId, Vec3 const&) = 0;

    BSCI_API virtual GeoId line(
        DimensionType        dim,
        std::span<Vec3>      dots,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    );

    BSCI_API virtual GeoId
    box(DimensionType        dim,
        AABB const&          box,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {});

    BSCI_API virtual GeoId circle(
        DimensionType        dim,
        Vec3 const&          center,
        Vec3 const&          normal,
        float                radius,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    );

    BSCI_API virtual GeoId cylinder(
        DimensionType        dim,
        Vec3 const&          topCenter,
        Vec3 const&          bottomCenter,
        float                radius,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    );

    BSCI_API virtual GeoId sphere(
        DimensionType        dim,
        Vec3 const&          center,
        float                radius,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    );

    BSCI_API virtual GeoId arrow(
        DimensionType        dim,
        Vec3 const&          begin,
        Vec3 const&          end,
        mce::Color const&    color            = mce::Color::WHITE(),
        std::optional<float> mArrowHeadLength = {},
        std::optional<float> mArrowHeadRadius = {}
    );

    BSCI_API virtual GeoId text(
        DimensionType        dim,
        Vec3 const&          pos,
        std::string          text,
        mce::Color const&    color = mce::Color::WHITE(),
        std::optional<float> scale = {}
    );

    BSCI_API virtual GeoId cone(
        DimensionType        dim,
        Vec3 const&          topCenter,
        Vec3 const&          bottomCenter,
        float                topRadius,
        float                bottomRadius,
        mce::Color const&    color     = mce::Color::WHITE(),
        std::optional<float> thickness = {}
    );
};
} // namespace bsci

namespace bsci::detail {
// viewer 为空：发送给 pos 附近的所有玩家；否则只发送给 viewer（viewer 不在线或不在 dim 维度时不发送）
void sendToViewerOrNearby(
    std::optional<mce::UUID> const& viewer,
    Packet const&                   packet,
    Vec3 const&                     pos,
    DimensionType                   dim
);
// viewer 为空：发送给所有玩家；否则只发送给 viewer
void sendToViewerOrAll(std::optional<mce::UUID> const& viewer, Packet const& packet);
// 判断 id 对应的客户端是否是 viewer
bool isViewerClient(mce::UUID const& viewer, NetworkIdentifier const& id);
} // namespace bsci::detail

namespace std {
template <>
struct hash<bsci::GeometryGroup::GeoId> {
    size_t operator()(bsci::GeometryGroup::GeoId const& id) {
        return std::hash<uint64>{}(id.value);
    }
};
} // namespace std
