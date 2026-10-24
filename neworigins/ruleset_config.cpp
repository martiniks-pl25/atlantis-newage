#include "ruleset_config.h"

// NewOrigins values for the typed ruleset parameters. Only what differs from the
// engine defaults in ruleset_config.h is listed here.
const RulesetConfig& ruleset_config()
{
    static constexpr RulesetConfig config {
        .pirates = {
            .treasure_map = { .decipher_chance = 30 },
            .recruit = { .intake_up = 4, .intake_down = 2, .pop_cost = 2 },
            .map_drop = { .chance_cap = 60, .hideout_soft_cap_percent = 7 },
            .spawn = { .elite_spawn_pct = 4 },
        },
        .dungeons = { .level = { .width = 128, .height = 32 } },
        .map = {
            .ocean_naming = { .enabled = true, .underground = true },
            .underworld_tunnels = { .enabled = true },
            .underground_lakes = { .landlocked_seas = true },
        },
        .mounts = { .water = {{ { .tag = "TURT", .max_bonus = 1 } }} },
    };
    static_assert(valid(config.pirates));
    static_assert(valid(config.quests));
    static_assert(valid(config.dungeons));
    static_assert(valid(config.guards));
    static_assert(valid(config.map));
    static_assert(valid(config.mounts));
    return config;
}
