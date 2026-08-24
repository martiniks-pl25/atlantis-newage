#include "external/boost/ut.hpp"
#include "game.h"
#include "gamedata.h"
#include "gamedefs.h"
#include "aregion.h"
#include "orders.h"
#include "testhelper.hpp"
#include "rng.hpp"

#include <array>
#include <filesystem>
#include <string>

namespace ut = boost::ut;

// The gazette must never draw its output filename from the game RNG. The world
// state depends on that stream, and the directory's contents are not part of the
// save, so a leftover times.* file would make a re-run of the same save draw a
// different number of random values and shift everything downstream (monster
// spawns, pirate fleets, promotions, quest targets). This test pins the class of
// bug — a gazette write consumes zero RNG draws — rather than the filename format.
ut::suite<"GazetteRngIsolation"> gazette_rng_isolation_suite = [] {
    using namespace ut;

    "Writing the gazette does not advance the game RNG stream"_test = [] {
        UnitTestHelper helper;

        // A distinctive turn number so this test only ever touches its own file
        // (year 1000, month 0 -> TurnNumber() == 11989).
        helper.game_object().year = 1000;
        helper.game_object().month = 0;

        constexpr int kDraws = 8;
        std::array<int, kDraws> before{};
        std::array<int, kDraws> after{};

        rng::seed_random(0x12345678);
        for (int i = 0; i < kDraws; ++i) before[i] = rng::get_random(1000000);

        rng::seed_random(0x12345678);
        helper.game_object().write_times_article("RNG isolation test article.");
        for (int i = 0; i < kDraws; ++i) after[i] = rng::get_random(1000000);

        expect(before == after) << "a gazette write must consume zero RNG draws";

        // Remove the file the write created so the test does not litter the
        // working directory. The turn number is distinctive, so only this test's
        // own file is removed.
        for (auto const &entry : std::filesystem::directory_iterator(".")) {
            auto name = entry.path().filename().string();
            if (name.rfind("times.11989.", 0) == 0) std::filesystem::remove(entry.path());
        }
    };
};
