#pragma once

#include <vector>

#include "mc/network/Packet.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/dimension/Dimension.h"

namespace bsci {

inline void sendToPlayerAt(
    Dimension&                  dim,
    BlockPos const&             pos,
    Packet const&               packet,
    std::vector<Player*> const& players
) {
    if (players.empty()) return dim.sendPacketForPosition(pos, packet, nullptr);

    for (auto* player : players) {
        if (player && player->getDimensionId() == dim.getDimensionId()
            && dim.is2DPositionRelevantForPlayer(pos, *player)) {
            packet.sendTo(*player);
        }
    }
}

} // namespace bsci