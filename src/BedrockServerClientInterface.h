#pragma once

#include <ll/api/plugin/NativePlugin.h>

#include "Config.h"

namespace bsci {

class BedrockServerClientInterface {
    struct Impl;
    std::unique_ptr<Impl> impl;

    ll::plugin::NativePlugin& self;

    std::optional<Config> mConfig;

public:
    BedrockServerClientInterface(ll::plugin::NativePlugin&);
    ~BedrockServerClientInterface();

    static BedrockServerClientInterface& getInstance();

    [[nodiscard]] ll::plugin::NativePlugin& getSelf() const;

    [[nodiscard]] ll::Logger& getLogger() const { return getSelf().getLogger(); }

    [[nodiscard]] Config& getConfig() { return *mConfig; }

    [[nodiscard]] std::filesystem::path getConfigPath() const;

    bool loadConfig();

    bool saveConfig();

    bool load();

    bool enable();

    bool disable();

    bool unload();
};

} // namespace bsci
