#pragma once

#include "BedrockServerClientInterface.h"

#ifdef TEST
#include "bsci/GeometryGroup.h"
#include <memory>

namespace bsci::test {
void registerTestCommand(
    std::unique_ptr<GeometryGroup>&    geo,
    std::vector<GeometryGroup::GeoId>& gids
);
}

#endif