#pragma once

#include "bsci/GeometryGroup.h"

#include <functional>
#include <memory>

class Packet;

namespace bsci {

class PacketGeoGroup : public GeometryGroup {
public:
    enum class PacketSendMode { Position, Broadcast };
    using PacketSender = std::function<
        void(GeoId, std::shared_ptr<Packet>, Vec3 const&, DimensionType, PacketSendMode)>;

protected:
    class Impl;
    std::shared_ptr<Impl> impl;

    PacketGeoGroup();

    void sendPacket(
        GeoId                   id,
        std::shared_ptr<Packet> packet,
        Vec3 const&             position,
        DimensionType           dimension,
        PacketSendMode          mode = PacketSendMode::Position
    ) const;

public:
    BSCI_API virtual ~PacketGeoGroup();

    BSCI_API void setPacketSender(PacketSender sender);
};

} // namespace bsci
