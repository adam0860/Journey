# Journey roadmap

The Game Design Document (kept in Claude Docs: "Classic RSC-Style Offline RPG - Game Design Document") is the source of
truth for WHAT the game is. This file is the working plan for building it: what is done, what is next, in what order.
Update it as work lands. (Adam asked for long, big batches of work, so the list below is deliberately long.)

## Done
- Castle Town, Millbrook village and the long road between, two art styles, character creator. (The world is now 192x400: see round 12.)
- Skills: woodcutting, fishing, firemaking, cooking, mining, smithing, prayer (bury bones), hitpoints, attack/strength/defence.
- Melee combat (turn based), animals, goblins, loot piles (60 s), coins, shops (9 shops), shopkeepers (People).
- Worn gear in four metals, weapon and shield animations.
- **Bank** (200 slots, shared by every bank, bankers behind the counter in Castle Town and Millbrook).
- **Archery**: 6 bows, 4 arrow metals, quiver slot, range 7-9 (+2 longrange), shoots over fences (`f` tiles), arrows drop.
- **Magic**: 6 runes, 5 staffs, wizard robes, spellbook tab, projectile effects.

## Done (continued)
- **Monsters + loot tables**: tiered loot (Always / Common / Uncommon / Rare), skills depth (oak/willow/yew, rods/harpoon/cages, 9+1 rock kinds, Crafting, cooking ranges).
- **Quests + dialogue**: dialogue box, quest points, kill / fetch / talk / reach steps.
- **Regions**: Lakeside (+ ferry), Wolf Wood, Troll Mountains, Mining Caves, Desert Ruins + Dragon's Temple, The Abyss, Castle Crypt.
- **Death rule** (Abyss: keep the 3 most valuable items), **teleport scrolls**, **ferries**, **world map tab**, **rings and amulets**.
- **Audio**: procedural music (5 moods) and 14 sound effects, volume settings in the Menu tab.

## Done in round 9 (the content round)
- **Adamant**, the fifth metal: ore, bar, 8 armour/weapon pieces, pickaxe, axe, arrows, adamant rocks (in the Abyss), smelting 70.
- **34 creature types** (the GDD's 30 + 4 fighters): cave spider, scorpion, jackal, hellhound, giant mole, hobgoblin, imp, baby dragon, black dragon, bandit, dark wizard, barbarian, ogre, skeleton warrior, black knight, hill giant. 237 drop rows.
- **30 quests** (6 new), **5 new fish** (sardine, herring, pike, bass, shark) with cooking, **diamonds and dragonstones** with jewellery, **unique jewellery** (Mummy's amulet, Troll-tooth amulet, Abyssal ring).
- **16 spells** (Blasts and Waves), the spellbook shows 8 to a page.
- **Save file version number** (`version=2`) and the places visited; teleport scrolls only work for places already visited.
- **How-to-play page** in the Menu tab; the Abyss is red on the map; worn amulets are drawn on the hero.
- Tests for the GDD's gates: content targets, "every quest can be finished" on the real map.

## Done in round 10 (the workshop round)
- **Items 179 -> 294**: seven more smithed types in all five metals (scimitar, longsword, spear, warhammer, full helm, square shield, chainbody), hard and studded leather, opal/jade/red topaz and the rings and amulets made from them.
- **Kitchen and woodwork**: dough, 5 pies, stew, cake, pizza (baked on a fire or range) and bows you carve and string yourself; ingredients at Hilda's Kitchen, bowstrings at Larson's.
- **Four more trees** (teak, maple, mahogany, magic), **five more fish** (mackerel, cod, eel, sea turtle, manta ray), rat and wolf meat.
- **"Use item" quest steps** (the design document's fifth step kind) and **6 more quests** (36): The Buried Chest, Light the Way, Hilda's Feast, The Bowyer's Pride, A Knight's Kit, The Ferryman's Feast.
- The smith's menu has pages; progress is saved on entering a new place, leaving the Abyss and quitting.
- **The design document's gates, as tests**: 10,000 simulated kills per table match the drop rates, every skill reaches level 10 in a simulated play-through (2-23 game minutes), every item has a model.

## Done in round 11
- **Weapon speeds** from the design document's tick (a dagger swings in 3 ticks, a warhammer in 6; slower weapons hit harder per swing), shown on the Gear tab.
- **Boots, gloves and capes**: three new equipment slots, smithed boots and gauntlets in five metals, leather ones, 14 skill capes and a quest cape (level 99 / all quests, 99,000 coins, from the king's Guild of Masters). Items: 324.
- **Quests open shop shelves** (staffs, yew bows, mithril armour, adamant ore...); **death sends you to the nearest town you have visited**; **Esc closes panels**.

## Done in round 12 (the wider world)
- **The map grew from 64 x 336 to 192 x 400**: the old strip now stands in the middle with country on every side (Adam: "everything scrolls vertically, expand east and west, add areas and biomes between the cities").
- **14 regions and 14 biomes** west and east of the strip and north of the castle: Whispering Pines, Elmwood, Greenmarch, Mirewood Swamp, Barrow Downs, Frostpeak Range (impassable peaks), Northern Reach, Northwatch Moor, Eastwatch Downs, the Sunfields, Darkwood, Giant's Rise, the Iron Range, the Dune Sea and the Eastern Sea. The edges between them (and into the old strip) are warped and dithered, not ruled lines.
- **Places**: Elmswick (woodcutters' village), Saltmere (harbour, pier, fishing), Sunfield Farm, Northwatch Tower, a bandit camp, standing stones, barrows, a witch's hut, an oasis, a woodcutters' camp. Roads, three bridges over the river, ~320 creatures in `w_*` pens, rocks of every kind, 8 new people, 5 new shops.
- **Streaming**: `TownRenderer` loads only the chunks near the hero (the 300-chunk world would otherwise take 12 s and a lot of memory); the HUD map follows the hero in both directions.
- Tests: `TestTheWiderWorld`, `TestNewTownsAreRespawnPoints`, `TestWiderWorldShops`; every existing real-map test moved to the shifted coordinates.

## Done in round 13 (other players)
- **The world is populated like an online game**: 8 real adventurers who start at level 1 beside the hero and train, quest, fight, smith and shop in real time by the same rules (their gear comes only from skilling, drops, quest rewards and shops), and 64 filler adventurers of every level from 3 to 99 with fixed gear who work, walk about and chat.
- Name tags, speech bubbles, chat-box lines, tap to examine / talk, "Players online" and the hiscores on the help page; saved with the hero's progress.

## Done in round 13 (continued)
- **Bonfires**: every kind of log can be lit as a campfire or, four at a time, as a bigger, longer-burning bonfire that cooks like a range: Firemaking has 16 actions (the design document's gate is 15 for every skill).
- **Per-weapon speeds for bows**: shortbows shoot every 3 ticks, longbows every 5 (and hit harder per shot).
- **Thrown darts** in five metals (the design document's "bows and thrown weapons"): thrown with an empty hand, range 5, one throw every 3 ticks; sold by Larson, dropped by goblins. Hit chance is now clamped to 5%-95% as the design document says.

## Done in round 13 (quality of life)
- **Teleport scrolls for Elmswick and Saltmere**, sold in the other towns (a scroll still needs the place visited).
- **Quest markers on the map**: gold diamonds at the next step of each quest in progress (a gold arrow on the edge of the close-up when it is far away), hollow diamonds over the people who have a quest for you; a test checks that every step of every quest has a place on the real map.
- **Scrollable quest log and how-to-play page** (drag or mouse wheel; a drag that starts on a panel never turns the camera).
- **Balance pass of the weapon types**: `tests/balance.h` + `build/Release/balance_report.exe` print the damage per second and the coin cost of every weapon; `TestWeaponBalance` keeps melee, bows, darts and spells in line. Magic is the strongest per second but costs runes; left as it is.

- **Eight more wild-country quests (53 in all)** using the people and places the wider world already has: Greenmarch Survey, Teak for the Cartwright, Deep Water Haul, Iron Range Run, Cold Comfort, A Night at the Stones, Harvest Help, The Witch's Errand.

- **The camera never sits behind a building**: it climbs over the roof when one is between it and the hero.

## Done in round 13 (the Abyss)
- **Player fights and the skull**: in the Abyss you can attack other adventurers; the first blow at someone not skulled (who did not start it) skulls you for ten minutes; a skulled death loses everything, which lies there for five minutes for anyone to take.
- **Abyss hunters** (6) who attack the hero on sight and take the dead's loot, and four miners and woodcutters working its rocks; the Abyss holds 20 adamant rocks, 8 gem rocks, 8 mithril rocks, 10 magic trees and 10 yew trees; five late-game quests.

- **Abyss levels**: the deeper in, the wider the gap of combat levels players can fight across (1 at the gate, 60 at the far end); real adventurers set out on expeditions, gather there, fight each other and the hunters, and take what the dead leave.

- **Abyss creatures attack the other players** too (and they fight back), real adventurers on an expedition take on creatures they can beat, and skulls are saved with the game.

- **Abyssal amulet** (the best amulet, a rare Abyss drop), a **kills/deaths record and hiscore** for the hero and the adventurers, and real adventurers who go to the Abyss **in pairs**.

## Next (ideas, roughly in order)
0. **The Abyss, further**: parties of three or more, a guild/clan feel for the pairs, Abyss events (a roaming boss other players race to).
1. **Balance by hand**: the table above says the weapon types are in line on paper; play a long session with each style (and each region's monsters) to check it feels right. Worn rings are still not drawn (too small to see).
3. **Polish**: better phone controls (a scrollable bank on small screens is not needed: it is paged).
4. **Balance pass** with real play-testing (XP rates, prices, monster levels, Abyss danger, adamant smelting cost). The training simulation (`TestEverySkillReachesLevelTen`) is the start: extend it to smithing/crafting at higher levels and to combat against each region's monsters.
5. **Prayer** beyond burying bones (the GDD puts it out of launch scope).
6. Android: test music/effects on the phone (miniaudio via OpenSL ES), the size of the style-B town models (~125 MB) and chunk streaming on a real phone.
7. **Use the wider world**: more per-region resources and quests that need new people; a quest-giver for each new person would need a Blender rebuild of the world (8 minutes per style).

## Rules of thumb
- Content is data (tables in `src/game/*.cpp`), rules are pure C++ with unit tests (`tests/test_core.cpp`), rendering only draws.
- Item ids are saved: only ever append to `enum Item`.
- Rebuild the town (Blender, both styles) in batches: it takes minutes.
