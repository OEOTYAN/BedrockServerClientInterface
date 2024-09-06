#include "BedrockServerClientInterface.h"

#include "bsci/GeometryGroup.h"

#include <ll/api/Config.h>
#include <ll/api/mod/NativeMod.h>
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/utils/ErrorUtils.h>

// #define TEST

namespace bsci {

struct BedrockServerClientInterface::Impl {};

BedrockServerClientInterface::BedrockServerClientInterface(ll::mod::NativeMod& self)
: self(self),
  impl(std::make_unique<Impl>()) {}

BedrockServerClientInterface::~BedrockServerClientInterface() = default;


static std::unique_ptr<BedrockServerClientInterface> instance;

BedrockServerClientInterface& BedrockServerClientInterface::getInstance() { return *instance; }

ll::mod::NativeMod& BedrockServerClientInterface::getSelf() const { return self; }

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
#ifdef TEST
    std::thread([] {
        auto                 geo = bsci::GeometryGroup::createDefault();
        GeometryGroup::GeoId eee{};
        auto gid = geo->circle(0, BlockPos{0, 90, 0}.center(), Vec3{1, 1, 1}.normalize(), 8);
        for (size_t i = 0;; i++) {
            using namespace std::chrono_literals;

            geo->shift(gid, {0, 0.01, 1});

            geo->remove(eee);

            eee = geo->sphere(0, BlockPos{0, 100, i}, 5, mce::Color{0, 33, 133, 50} * 1.6);

            std::this_thread::sleep_for(1s);
        }
    }).detach();
#endif
    return true;
}

bool BedrockServerClientInterface::disable() {
    saveConfig();
    return true;
}

bool BedrockServerClientInterface::unload() { return true; }
} // namespace bsci
LL_REGISTER_MOD(bsci::BedrockServerClientInterface, bsci::instance);
