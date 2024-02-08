#pragma once

#include <ll/api/plugin/NativePlugin.h>

namespace bsci {

class BedrockServerClientInterface {
    struct Impl;
    std::unique_ptr<Impl> impl;

    ll::plugin::NativePlugin* self{};


    BedrockServerClientInterface();
    ~BedrockServerClientInterface();

public:
    BedrockServerClientInterface(BedrockServerClientInterface&&)                 = delete;
    BedrockServerClientInterface(const BedrockServerClientInterface&)            = delete;
    BedrockServerClientInterface& operator=(BedrockServerClientInterface&&)      = delete;
    BedrockServerClientInterface& operator=(const BedrockServerClientInterface&) = delete;

    static BedrockServerClientInterface& getInstance();

    [[nodiscard]] ll::plugin::NativePlugin& getSelf() const;

    bool load(ll::plugin::NativePlugin&);

    bool enable();

    bool disable();

    bool unload();
};

} // namespace bsci
