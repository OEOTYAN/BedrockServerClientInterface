#pragma once

namespace bsci {
struct Config {
    int version = 2;
    struct {
        size_t maxCircleSegments  = 128;
        double minCircleSpacing   = 0.6;
        size_t maxSphereCells     = 10;
        double minSphereSpacing   = 0.6;
        double extraTime          = 0.05;
        size_t tablePerTick       = 2;
        double defaultThickness   = 0.1;
        double defaultPointRadius = 0.3;
        bool   delayUndate        = false;
    } particle{};
};
} // namespace bsci