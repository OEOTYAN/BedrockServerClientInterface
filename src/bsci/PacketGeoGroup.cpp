#include "PacketGeoGroup.h"

#include <utility>

#include <ll/api/thread/ServerThreadExecutor.h>
#include <mc/network/Packet.h>

namespace bsci {
namespace {

PacketGeoGroup::PacketSender defaultPacketSender() {
    return [](PacketGeoGroup::GeoId /*id*/,
              std::shared_ptr<Packet>       packet,
              Vec3 const&                   position,
              DimensionType                 dimension,
              PacketGeoGroup::PacketSendMode mode) {
        if (!packet) return;

        ll::thread::ServerThreadExecutor::getDefault().execute(
            [packet = std::move(packet), position, dimension, mode] {
                if (mode == PacketGeoGroup::PacketSendMode::Broadcast) {
                    packet->sendToClients();
                } else {
                    packet->sendTo(position, dimension);
                }
            }
        );
    };
}

} // namespace

class PacketGeoGroup::Impl {
public:
    PacketSender sendPacket;

    Impl() : sendPacket(defaultPacketSender()) {}
};

PacketGeoGroup::PacketGeoGroup() : impl(std::make_shared<Impl>()) {}

PacketGeoGroup::~PacketGeoGroup() = default;

void PacketGeoGroup::sendPacket(
    GeoId                   id,
    std::shared_ptr<Packet> packet,
    Vec3 const&             position,
    DimensionType           dimension,
    PacketSendMode          mode
) const {
    if (!packet) return;
    impl->sendPacket(id, std::move(packet), position, dimension, mode);
}

void PacketGeoGroup::setPacketSender(PacketSender sender) {
    impl->sendPacket = sender ? std::move(sender) : defaultPacketSender();
}

} // namespace bsci
