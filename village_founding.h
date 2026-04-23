#pragma once

/// Constants and types for the CREATE VILLAGE order.
/// Shared between monthorders.cpp (execution) and genrules.cpp (rule generation).

/// Number of people (IT_MAN + IT_LEADER) required and consumed to found a village.
inline constexpr int VILLAGE_FOUND_MEN = 1000;

/// Describes an additional item cost for founding a village.
struct SettlementCost {
    int item;        ///< Item ID (I_WAGON, etc.), or -1 to terminate the array.
    int amount;      ///< Required quantity.
    const char *error_msg; ///< Error shown to player if quantity insufficient.
};

/// Array of additional item costs (wagons, etc.) for founding a village.
/// Terminated by a sentinel entry with item == -1.
/// Defined in monthorders.cpp; add entries there to tune costs.
extern const SettlementCost VILLAGE_ITEM_COSTS[];
