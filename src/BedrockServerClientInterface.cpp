#include "BedrockServerClientInterface.h"


#include <ll/api/Config.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/command/ServerCommandRegisterEvent.h>
#include <ll/api/mod/NativeMod.h>
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/ErrorUtils.h>

#ifdef TEST
#include "bsci/GeometryGroup.h"
#include "bsci/test/Test.h"
#endif

namespace bsci {

struct BedrockServerClientInterface::Impl {
#ifdef TEST
    std::unique_ptr<GeometryGroup>    geoTest;
    std::vector<GeometryGroup::GeoId> gids;
    std::set<ll::event::ListenerPtr>  eventListeners;
#endif
};

BedrockServerClientInterface::BedrockServerClientInterface()
: impl(std::make_unique<Impl>()),
  self(*ll::mod::NativeMod::current()) {}

BedrockServerClientInterface::~BedrockServerClientInterface() = default;

BedrockServerClientInterface& BedrockServerClientInterface::getInstance() {
    static BedrockServerClientInterface instance;
    return instance;
}

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
#ifdef TEST
    impl->eventListeners.emplace(
        ll::event::EventBus::getInstance()
            .emplaceListener<ll::event::command::ServerCommandRegisterEvent>([this](auto&&) {
                getLogger().info("registering test command");
                impl->geoTest = GeometryGroup::createDefault();
                test::registerTestCommand(impl->geoTest, impl->gids);
            })
    );
#endif
    return true;
}

bool BedrockServerClientInterface::enable() {
    if (!mConfig) {
        loadConfig();
    }
#ifdef TEST
#ifdef LL_PLAT_C
    if (ll::service::getLevel()) {
        impl->geoTest = GeometryGroup::createDefault();
        test::registerTestCommand(impl->geoTest, impl->gids);
    }
#endif
    std::thread([] {
        auto                 geo = bsci::GeometryGroup::createDefault();
        GeometryGroup::GeoId eee{};
        auto gid = geo->circle(0, BlockPos{0, 90, 0}.center(), Vec3{1, 1, 1}.normalize(), 8);
        for (size_t i = 0;; i++) {
            using namespace std::chrono_literals;

            geo->shift(gid, {0, 0.01, 1});

            geo->remove(eee);

            eee = geo->sphere(0, BlockPos{0, 100, i}, 7, mce::Color{0, 33, 133, 50} * 1.6);

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

LL_REGISTER_MOD(
    bsci::BedrockServerClientInterface,
    bsci::BedrockServerClientInterface::getInstance()
);
