# BedrockServerClientInterface

## Use

Add in xmake.lua

```lua
add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")
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

## Contributing

Ask questions by creating an issue.

PRs accepted.

## License

AGPL v3.0
