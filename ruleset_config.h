#pragma once

#include <algorithm>

/**
 * @file ruleset_config.h
 * @brief Typed ruleset parameters, grouped by subsystem.
 *
 * The engine declares the structs and their defaults; the defaults describe how the
 * engine behaves with no ruleset opinion. A ruleset provides ruleset_config() and
 * overrides only what differs, by field name with C++20 designated initializers:
 *
 * @code
 *   const RulesetConfig& ruleset_config() {
 *       static const RulesetConfig config {
 *           .pirates = { .treasure_map = { .decipher_chance = 30 } },
 *       };
 *       return config;
 *   }
 * @endcode
 *
 * New tunables go here rather than into GameDefs (positional initializer), the C
 * globals in rules.cpp or Game::rulesetSpecificData (untyped string keys). Player
 * text that states a number reads it from here too, so text and mechanics cannot
 * drift apart.
 *
 * @see neworigins/ruleset_config.cpp, docs/ARCHITECTURE.md ("Ruleset config")
 */

/**
 * @brief EXPLORE TMAP: deciphering a treasure map to find a pirate hideout.
 */
struct TreasureMapRules {
    int decipher_chance    = 25;  ///< % per month of study to decipher the map
    int compass_multiplier = 2;   ///< decipher_chance is multiplied by this with a compass
    int burn_on_fail       = 50;  ///< % a failed attempt destroys the map

    /**
     * @brief Chance, in percent, that one month of EXPLORE TMAP deciphers the map.
     * @param has_compass whether the exploring unit carries a compass
     * @return decipher_chance, times compass_multiplier with a compass, capped at 100
     */
    [[nodiscard]] constexpr int chance(const bool has_compass) const noexcept {
        const int c = has_compass ? decipher_chance * compass_multiplier : decipher_chance;
        return std::clamp(c, 0, 100);
    }
};

/// Pirate tunables.
struct PirateRules {
    TreasureMapRules treasure_map;
};

/// All typed ruleset parameters.
struct RulesetConfig {
    PirateRules pirates;
};

/**
 * @brief The active ruleset's parameters. Defined by the ruleset, read-only.
 */
[[nodiscard]] const RulesetConfig& ruleset_config();
