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
        },
    };
    static_assert(valid(config.pirates));
    static_assert(valid(config.quests));
    return config;
}
