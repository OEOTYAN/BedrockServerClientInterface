#include "BedrockServerClientInterface.h"

#include "bsci/GeometryGroup.h"

#include <ll/api/Config.h>
#include <ll/api/plugin/NativePlugin.h>
#include <ll/api/plugin/RegisterHelper.h>
#include <ll/api/utils/ErrorUtils.h>

namespace bsci {

struct BedrockServerClientInterface::Impl {};

BedrockServerClientInterface::BedrockServerClientInterface(ll::plugin::NativePlugin& self)
: self(self),
  impl(std::make_unique<Impl>()) {}

BedrockServerClientInterface::~BedrockServerClientInterface() = default;


static std::unique_ptr<BedrockServerClientInterface> instance;

BedrockServerClientInterface& BedrockServerClientInterface::getInstance() { return *instance; }

ll::plugin::NativePlugin& BedrockServerClientInterface::getSelf() const { return self; }

std::filesystem::path BedrockServerClientInterface::getConfigPath() const {
    return getSelf().getConfigDir() / u8"config.json";
}

bool BedrockServerClientInterface::loadConfig() {
    bool res{};
    mConfig.emplace();
    try {
        res = ll::config::loadConfig(*mConfig, getConfigPath());
    } catch (...) {
        ll::error_utils::printCurrentException(getLogger());
        res = false;
    }
    if (!res) {
        res = ll::config::saveConfig(*mConfig, getConfigPath());
    }
    return res;
}

bool BedrockServerClientInterface::saveConfig() {
    return ll::config::saveConfig(*mConfig, getConfigPath());
}

bool BedrockServerClientInterface::load() {
    if (!loadConfig()) {
        return false;
    }
    return true;
}

bool BedrockServerClientInterface::enable() {
    if (!mConfig) {
        loadConfig();
    }
    return true;
}

bool BedrockServerClientInterface::disable() {
    saveConfig();
    return true;
}

bool BedrockServerClientInterface::unload() { return true; }
} // namespace bsci
LL_REGISTER_PLUGIN(bsci::BedrockServerClientInterface, bsci::instance);
