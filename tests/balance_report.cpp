// A development tool, not a test: prints the damage per second of every weapon (melee, bows, darts, spells) against a creature of
// about the hero's own level, so the weapon types can be compared and balanced. Run it after changing gear, arrows, spells or combat.
//   build/Release/balance_report.exe          (no arguments)
// The guard rails that keep it balanced are in tests/test_core.cpp (TestWeaponBalance); the table itself is in tests/balance.h.
#include <algorithm>
#include <cstdio>

#include "balance.h"

int main() {
    std::vector<balance::Row> rows = balance::WeaponTable();
    std::stable_sort(rows.begin(), rows.end(), [](const balance::Row& a, const balance::Row& b) { return a.cls != b.cls ? a.cls < b.cls : a.level < b.level; });
    std::printf("%-6s %-34s %5s %7s %6s %6s %7s %9s\n", "class", "weapon", "level", "maxhit", "hit%", "secs", "dps", "coins/min");
    for (const balance::Row& r : rows)
        std::printf("%-6s %-34s %5d %7.1f %6.1f %6.1f %7.2f %9.1f\n", r.cls.c_str(), r.name.c_str(), r.level, r.maxHit, r.hit * 100.0, r.seconds, r.dps,
                    r.CoinsPerMinute());
    std::printf("\nbest damage per second usable at each level (melee / bow or dart / spell):\n");
    for (int level : {1, 5, 10, 20, 30, 40, 45})
        std::printf("  level %2d: %5.2f / %5.2f / %5.2f\n", level, balance::BestDps(rows, "melee", level),
                    std::max(balance::BestDps(rows, "bow", level), balance::BestDps(rows, "dart", level)), balance::BestDps(rows, "spell", level));
    return 0;
}
