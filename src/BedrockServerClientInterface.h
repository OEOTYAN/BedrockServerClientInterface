#pragma once

#include <ll/api/mod/NativeMod.h>

#include "Config.h"

namespace bsci {

class BedrockServerClientInterface {
    struct Impl;
    std::unique_ptr<Impl> impl;

    ll::mod::NativeMod& self;

    std::optional<Config> mConfig;

public:
    BedrockServerClientInterface(ll::mod::NativeMod&);
    ~BedrockServerClientInterface();

    static BedrockServerClientInterface& getInstance();

    [[nodiscard]] ll::mod::NativeMod& getSelf() const;

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
