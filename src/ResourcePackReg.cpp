#include "BedrockServerClientInterface.h"

#include <ll/api/memory/Hook.h>

#include <mc/network/packet/ResourcePacksInfoPacket.h>
#include <mc/resources/ResourcePackRepository.h>

namespace bsci {

// LL_AUTO_TYPE_INSTANCE_HOOK(
//     ResourceInitHook,
//     ll::memory::HookPriority::Normal,
//     ResourcePackRepository,
//     &ResourcePackRepository::_initialize,
//     void
// ) {
//     this->addCustomResourcePackPath(
//         BedrockServerClientInterface::getInstance().getSelf().getModDir()
//             / u8"resource_packs/BSCIPack",
//         PackType::Resources
//     );
//     origin();
// }
// LL_AUTO_TYPE_INSTANCE_HOOK(
//     ResourcePacksInfoHook,
//     ll::memory::HookPriority::Normal,
//     ResourcePacksInfoPacket,
//     "?write@ResourcePacksInfoPacket@@UEBAXAEAVBinaryStream@@@Z",
//     void,
//     BinaryStream& stream
// ) {
//     mData.mForceServerPacksEnabled = false;
//     origin(stream);
// }
} // namespace bsci