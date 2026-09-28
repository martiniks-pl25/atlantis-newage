#include "ruleset_config.h"

// Stub ruleset: the engine defaults, unchanged.
const RulesetConfig& ruleset_config()
{
    static constexpr RulesetConfig config {};
    static_assert(valid(config.pirates));
    static_assert(valid(config.quests));
    static_assert(valid(config.dungeons));
    static_assert(valid(config.map));
    return config;
}
