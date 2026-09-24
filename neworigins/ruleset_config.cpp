#include "ruleset_config.h"

// NewOrigins values for the typed ruleset parameters. Only what differs from the
// engine defaults in ruleset_config.h is listed here.
const RulesetConfig& ruleset_config()
{
    static constexpr RulesetConfig config {
        .pirates = {
            .treasure_map = { .decipher_chance = 30 },
        },
    };
    return config;
}
