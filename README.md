# BedrockServerClientInterface

## Use

Add in xmake.lua

```lua
add_repositories("OTOTYAN https://github.com/OEOTYAN/xmake-repo.git")
add_requires("bsci")

```

example

```cpp
#include <bsci/GeometryGroup.h>

int main() {

    static auto geo = bsci::GeometryGroup::createDefault();

    geo->line(0, BlockPos{0, 70, 0}.center(), BlockPos{10, 73, 6}.center(), mce::Color::BLUE);

    geo->box(
        0,
        BoundingBox{
            {0,  70, 0},
            {10, 73, 6}
    },
        mce::Color::PINK
    );

    geo->circle(0, BlockPos{0, 73, 0}.center(), Vec3{1, 1, 1}.normalize(), 8);

    auto s = geo->sphere(0, BlockPos{0, 180, 0}.center(), 100, mce::Color::CYAN);

    std::thread([&, s] {
        std::this_thread::sleep_for(std::chrono::seconds{30});
        geo->remove(s);
    }).join();

    return 0;
}

```

### Target specific players

Every drawing call accepts an optional `players` argument. Leave it empty (the default) to send
to all relevant players, or pass a list of players to limit the update to them:

```cpp
std::vector<Player*> players{player};

geo->box(0, BoundingBox{{0, 70, 0}, {10, 73, 6}}, mce::Color::PINK, {}, players);

// A single player can also be passed directly:
geo->point(0, BlockPos{0, 70, 0}.center(), mce::Color::RED, {}, {player});
```

The target list is remembered by the geometry, so later `shift` / `remove` updates are only sent to
the same players.

## Contributing

Ask questions by creating an issue.

PRs accepted.

## License

AGPL v3.0
