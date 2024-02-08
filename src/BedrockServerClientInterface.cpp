#include "BedrockServerClientInterface.h"

#include "particle/ParticleSender.h"

#include <ll/api/plugin/NativePlugin.h>
#include <ll/api/schedule/Scheduler.h>
#include <memory>

namespace bsci {

struct BedrockServerClientInterface::Impl {
    std::optional<ll::schedule::ServerTimeScheduler> scheduler;
};

BedrockServerClientInterface::BedrockServerClientInterface() : impl(std::make_unique<Impl>()) {
    impl->scheduler.emplace();
}

BedrockServerClientInterface::~BedrockServerClientInterface() = default;

BedrockServerClientInterface& BedrockServerClientInterface::getInstance() {
    static BedrockServerClientInterface instance;
    return instance;
}

ll::plugin::NativePlugin& BedrockServerClientInterface::getSelf() const { return *self; }

bool BedrockServerClientInterface::load(ll::plugin::NativePlugin& plugin) {
    self = std::addressof(plugin);
    return true;
}

bool BedrockServerClientInterface::enable() {
    using namespace ll::chrono_literals;

    impl->scheduler->add<ll::schedule::RepeatTask>(1s, [] {
        static ParticleSender p(std::chrono::duration_cast<std::chrono::duration<float>>(1s).count()
        );
        p.line(0, BlockPos{0, 70, 0}.center(), BlockPos{10, 73, 6}.center(), mce::Color::BLUE);
        p.box(
            0,
            BoundingBox{
                {0,  70, 0},
                {10, 73, 6}
        },
            mce::Color::PINK
        );
    });

    return true;
}

bool BedrockServerClientInterface::disable() {
    impl->scheduler.reset();
    return true;
}

bool BedrockServerClientInterface::unload() {
    self = nullptr;
    return true;
}

extern "C" {
_declspec(dllexport) bool ll_plugin_load(ll::plugin::NativePlugin& self) {
    return BedrockServerClientInterface::getInstance().load(self);
}
_declspec(dllexport) bool ll_plugin_enable(ll::plugin::NativePlugin&) {
    return BedrockServerClientInterface::getInstance().enable();
}
_declspec(dllexport) bool ll_plugin_disable(ll::plugin::NativePlugin&) {
    return BedrockServerClientInterface::getInstance().disable();
}
_declspec(dllexport) bool ll_plugin_unload(ll::plugin::NativePlugin&) {
    return BedrockServerClientInterface::getInstance().unload();
}
}

} // namespace bsci
