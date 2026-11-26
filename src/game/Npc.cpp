#include "game/Npc.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "game/Gear.h"
#include "game/Loot.h"
#include "world/Pathfinder.h"

namespace journey {

namespace {

// A new creature is one entry here (plus its loot table in Loot.cpp, and a model if it is not one of the existing rigs).
SpeciesInfo Make(const char* key, const char* name, int level, int hp, int att, int str, int def, float speed, float respawn,
                 const char* examine, const char* loot, Rig rig, float height) {
    SpeciesInfo s{key, name, level, hp, att, str, def, speed, respawn, examine, loot};
    s.rig = rig;
    s.height = height;
    return s;
}

std::vector<SpeciesInfo> BuildSpecies() {
    std::vector<SpeciesInfo> v;
    // ---- the farm
    SpeciesInfo chicken = Make("chicken", "Chicken", 1, 3, 1, 1, 1, 1.9f, 25.0f, "Yep, definitely a chicken.", "chicken", Rig::Bird, 0.55f);
    SpeciesInfo cow = Make("cow", "Cow", 3, 6, 3, 3, 3, 1.4f, 45.0f, "Converts grass to beef.", "cow", Rig::Quad, 1.45f);
    SpeciesInfo calf = Make("calf", "Calf", 2, 4, 2, 1, 2, 2.1f, 35.0f, "A young cow.", "calf", Rig::Quad, 0.95f);
    // ---- goblins: aggressive, small, they carry a few coins
    SpeciesInfo goblin = Make("goblin", "Goblin", 2, 5, 2, 2, 2, 1.7f, 40.0f, "An ugly green creature with a bad attitude.", "goblin", Rig::Biped, 1.25f);
    goblin.aggressive = true;
    goblin.aggroRange = 3;
    goblin.model = "goblin";
    goblin.colorway = 0;
    SpeciesInfo warrior = Make("goblin_warrior", "Goblin warrior", 5, 10, 5, 5, 4, 1.6f, 70.0f, "A big goblin in scraps of armour.", "goblin_warrior", Rig::Biped, 1.45f);
    warrior.aggressive = true;
    warrior.aggroRange = 3;
    warrior.model = "goblin";
    warrior.colorway = 1;
    warrior.scale = 1.15f;
    // ---- animals of the wild
    SpeciesInfo rat = Make("giant_rat", "Giant rat", 3, 5, 3, 2, 2, 2.2f, 30.0f, "Overgrown vermin.", "giant_rat", Rig::Quad, 0.55f);
    rat.aggressive = true;
    rat.aggroRange = 2;
    rat.model = "rat";
    rat.colorway = -1;
    SpeciesInfo wolf = Make("wolf", "Wolf", 11, 16, 10, 9, 8, 2.4f, 60.0f, "A grey wolf. It hunts in a pack.", "wolf", Rig::Quad, 1.0f);
    wolf.aggressive = true;
    wolf.aggroRange = 4;
    wolf.pack = 6;
    wolf.model = "wolf";
    wolf.colorway = 0;
    SpeciesInfo dark = Make("dark_wolf", "Dark wolf", 35, 42, 32, 30, 28, 2.7f, 120.0f, "A huge black wolf with burning eyes.", "dark_wolf", Rig::Quad, 1.2f);
    dark.aggressive = true;
    dark.aggroRange = 5;
    dark.pack = 7;
    dark.model = "wolf";
    dark.colorway = 1;
    dark.scale = 1.25f;
    SpeciesInfo spider = Make("giant_spider", "Giant spider", 15, 20, 14, 13, 12, 2.2f, 60.0f, "Its bite is poisonous.", "giant_spider", Rig::Spider, 0.9f);
    spider.aggressive = true;
    spider.aggroRange = 3;
    spider.poison = 2;
    spider.model = "spider";
    spider.colorway = -1;
    // ---- the undead and the big ones (drawn with the hero's own body, tinted)
    SpeciesInfo zombie = Make("zombie", "Zombie", 13, 24, 12, 12, 10, 1.2f, 60.0f, "The walking dead.", "zombie", Rig::Human, 1.8f);
    zombie.aggressive = true;
    zombie.aggroRange = 3;
    zombie.look = {false, 2, 0, 3, 0, 4, 0, 5, 1, 4};
    zombie.tintSkin = true;
    zombie.skinTint = {118, 148, 92};
    zombie.bald = false;
    zombie.tintShirt = zombie.tintPants = true;
    zombie.shirtTint = {96, 92, 76};
    zombie.pantsTint = {70, 66, 56};
    SpeciesInfo skeleton = Make("skeleton", "Skeleton", 25, 30, 24, 22, 20, 1.8f, 75.0f, "A pile of angry bones.", "skeleton", Rig::Human, 1.8f);
    skeleton.aggressive = true;
    skeleton.aggroRange = 4;
    skeleton.look = {false, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    skeleton.tintSkin = skeleton.bald = true;
    skeleton.skinTint = {226, 220, 200};
    skeleton.weapon = GearItem(Tier::Iron, GearType::Sword);
    skeleton.shield = GearItem(Tier::Bronze, GearType::Shield);
    skeleton.tintShirt = skeleton.tintPants = true;   // (no clothes: the shirt and trousers are bone coloured too, so only a ragged body shows)
    skeleton.shirtTint = {204, 198, 178};
    skeleton.pantsTint = {190, 184, 164};
    SpeciesInfo troll = Make("mountain_troll", "Mountain troll", 50, 78, 50, 52, 45, 1.5f, 150.0f, "A huge grey brute with a club.", "mountain_troll", Rig::Human, 3.0f);
    troll.aggressive = true;
    troll.aggroRange = 3;
    troll.scale = 1.65f;
    troll.look = {false, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    troll.tintSkin = true;
    troll.skinTint = {132, 138, 124};
    troll.tintShirt = true;
    troll.shirtTint = {96, 70, 44};
    troll.tintPants = true;
    troll.pantsTint = {82, 60, 38};
    troll.weapon = GearItem(Tier::Iron, GearType::Mace);
    SpeciesInfo mummy = Make("desert_mummy", "Desert mummy", 70, 100, 62, 60, 58, 1.4f, 180.0f, "Wrapped in ancient bandages. It throws bones.", "desert_mummy", Rig::Human, 1.9f);
    mummy.aggressive = true;
    mummy.aggroRange = 6;
    mummy.attackKind = AttackKind::Ranged;
    mummy.attackRange = 6;
    mummy.look = {false, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    mummy.tintSkin = mummy.tintShirt = mummy.tintPants = mummy.bald = true;
    mummy.skinTint = mummy.shirtTint = {214, 200, 164};
    mummy.pantsTint = {196, 182, 146};
    mummy.weapon = Item::OakLongbow;
    SpeciesInfo dragon = Make("dragon", "Dragon", 90, 260, 85, 85, 80, 1.6f, 600.0f, "A huge, fire-breathing dragon. Run.", "dragon", Rig::Dragon, 3.4f);
    dragon.aggressive = true;
    dragon.aggroRange = 7;
    dragon.attackKind = AttackKind::Magic;
    dragon.attackRange = 7;
    dragon.spellColor = {255, 90, 20};
    dragon.model = "dragon";
    dragon.colorway = 0;
    // ---- the Abyss: fighters in the hero's own armour
    SpeciesInfo aw = Make("abyss_warrior", "Abyss warrior", 40, 40, 40, 38, 36, 2.0f, 90.0f, "A wanderer of the Abyss, armed to the teeth.", "abyss_warrior", Rig::Human, 1.85f);
    aw.aggressive = true;
    aw.aggroRange = 5;
    aw.look = {false, 3, 3, 1, 0, 9, 0, 13, 1, 2};
    aw.weapon = GearItem(Tier::Steel, GearType::Sword);
    aw.shield = GearItem(Tier::Steel, GearType::Shield);
    aw.helm = GearItem(Tier::Steel, GearType::Helm);
    aw.body = GearItem(Tier::Steel, GearType::Body);
    aw.legs = GearItem(Tier::Steel, GearType::Legs);
    SpeciesInfo aa = Make("abyss_archer", "Abyss archer", 38, 36, 36, 36, 30, 2.1f, 90.0f, "A wanderer of the Abyss with a bow.", "abyss_archer", Rig::Human, 1.85f);
    aa.aggressive = true;
    aa.aggroRange = 7;
    aa.attackKind = AttackKind::Ranged;
    aa.attackRange = 7;
    aa.look = {true, 2, 5, 2, 1, 6, 0, 3, 1, 1};
    aa.weapon = Item::OakLongbow;
    aa.helm = GearItem(Tier::Iron, GearType::Helm);
    aa.body = GearItem(Tier::Iron, GearType::Body);
    SpeciesInfo am = Make("abyss_mage", "Abyss mage", 36, 32, 34, 34, 28, 2.0f, 90.0f, "A wanderer of the Abyss, crackling with magic.", "abyss_mage", Rig::Human, 1.85f);
    am.aggressive = true;
    am.aggroRange = 7;
    am.attackKind = AttackKind::Magic;
    am.attackRange = 7;
    am.spellColor = {255, 120, 30};
    am.look = {false, 1, 7, 7, 1, 8, 0, 8, 1, 2};
    am.weapon = Item::FireStaff;
    am.helm = Item::WizardHat;
    am.body = Item::WizardRobe;
    am.legs = Item::WizardSkirt;
    SpeciesInfo ac = Make("abyss_champion", "Abyss champion", 60, 110, 62, 62, 60, 2.0f, 300.0f, "The terror of the Abyss, in mithril from head to toe.", "abyss_champion", Rig::Human, 1.9f);
    ac.aggressive = true;
    ac.aggroRange = 6;
    ac.scale = 1.1f;
    ac.look = {false, 4, 1, 0, 0, 10, 0, 13, 1, 3};
    ac.weapon = GearItem(Tier::Mithril, GearType::Sword);
    ac.shield = GearItem(Tier::Mithril, GearType::Shield);
    ac.helm = GearItem(Tier::Mithril, GearType::Helm);
    ac.body = GearItem(Tier::Mithril, GearType::Body);
    ac.legs = GearItem(Tier::Mithril, GearType::Legs);
    // ---- more animals: the same bodies as above, in other colourways and sizes
    SpeciesInfo cave = Make("cave_spider", "Cave spider", 6, 9, 6, 5, 4, 2.4f, 40.0f, "A small, pale spider that lives in the dark.", "cave_spider", Rig::Spider, 0.6f);
    cave.aggressive = true;
    cave.aggroRange = 2;
    cave.poison = 1;
    cave.model = "spider";
    cave.colorway = 2;
    cave.scale = 0.6f;
    SpeciesInfo scorpion = Make("scorpion", "Scorpion", 28, 34, 27, 26, 24, 2.3f, 70.0f, "Its tail ends in a nasty sting.", "scorpion", Rig::Spider, 0.9f);
    scorpion.aggressive = true;
    scorpion.aggroRange = 3;
    scorpion.poison = 3;
    scorpion.model = "spider";
    scorpion.colorway = 3;
    scorpion.scale = 0.95f;
    SpeciesInfo jackal = Make("jackal", "Jackal", 18, 22, 17, 16, 14, 2.5f, 60.0f, "A scruffy desert dog. It hunts in a pack.", "jackal", Rig::Quad, 0.85f);
    jackal.aggressive = true;
    jackal.aggroRange = 4;
    jackal.pack = 5;
    jackal.model = "wolf";
    jackal.colorway = 2;
    jackal.scale = 0.85f;
    SpeciesInfo hound = Make("hellhound", "Hellhound", 62, 95, 60, 60, 56, 2.8f, 240.0f, "A hound of the Abyss. Its eyes burn.", "hellhound", Rig::Quad, 1.5f);
    hound.aggressive = true;
    hound.aggroRange = 6;
    hound.pack = 4;
    hound.model = "wolf";
    hound.colorway = 3;
    hound.scale = 1.35f;
    SpeciesInfo mole = Make("giant_mole", "Giant mole", 22, 30, 21, 21, 18, 1.6f, 90.0f, "A huge mole that has tunnelled through the caves.", "giant_mole", Rig::Quad, 1.1f);
    mole.aggressive = true;
    mole.aggroRange = 2;
    mole.model = "rat";
    mole.colorway = 2;
    mole.scale = 2.4f;
    SpeciesInfo hobgoblin = Make("hobgoblin", "Hobgoblin", 16, 26, 15, 15, 13, 1.7f, 60.0f, "A big, ugly goblin who bullies the little ones.", "hobgoblin", Rig::Biped, 1.6f);
    hobgoblin.aggressive = true;
    hobgoblin.aggroRange = 4;
    hobgoblin.model = "goblin";
    hobgoblin.colorway = 2;
    hobgoblin.scale = 1.3f;
    SpeciesInfo imp = Make("imp", "Imp", 8, 9, 7, 6, 5, 2.4f, 40.0f, "A little red troublemaker.", "imp", Rig::Biped, 0.9f);
    imp.aggressive = true;
    imp.aggroRange = 3;
    imp.model = "goblin";
    imp.colorway = 3;
    imp.scale = 0.75f;
    SpeciesInfo baby = Make("baby_dragon", "Baby dragon", 45, 70, 42, 42, 38, 1.9f, 150.0f, "A young dragon. It is not as small as it sounds.", "baby_dragon", Rig::Dragon, 1.8f);
    baby.aggressive = true;
    baby.aggroRange = 5;
    baby.attackKind = AttackKind::Magic;
    baby.attackRange = 4;
    baby.spellColor = {255, 110, 30};
    baby.model = "dragon";
    baby.colorway = 0;
    baby.scale = 0.55f;
    SpeciesInfo black = Make("black_dragon", "Black dragon", 78, 190, 74, 76, 72, 1.7f, 480.0f, "A black dragon that breathes dark fire.", "black_dragon", Rig::Dragon, 3.0f);
    black.aggressive = true;
    black.aggroRange = 7;
    black.attackKind = AttackKind::Magic;
    black.attackRange = 7;
    black.spellColor = {170, 60, 255};
    black.model = "dragon";
    black.colorway = 1;
    black.scale = 0.9f;
    // ---- more people-shaped fighters
    SpeciesInfo bandit = Make("bandit", "Bandit", 17, 25, 16, 15, 13, 1.9f, 70.0f, "A road robber. Mind your purse.", "bandit", Rig::Human, 1.8f);
    bandit.aggressive = true;
    bandit.aggroRange = 4;
    bandit.look = {false, 3, 3, 1, 0, 5, 0, 2, 1, 0};
    bandit.tintShirt = bandit.tintPants = true;
    bandit.shirtTint = {64, 52, 48};
    bandit.pantsTint = {44, 40, 38};
    bandit.weapon = GearItem(Tier::Iron, GearType::Sword);
    bandit.body = Item::LeatherBody;
    SpeciesInfo wizard = Make("dark_wizard", "Dark wizard", 20, 24, 19, 19, 12, 1.8f, 80.0f, "A wizard who went bad. He throws earth.", "dark_wizard", Rig::Human, 1.8f);
    wizard.aggressive = true;
    wizard.aggroRange = 5;
    wizard.attackKind = AttackKind::Magic;
    wizard.attackRange = 5;
    wizard.spellColor = {140, 90, 60};
    wizard.look = {false, 1, 7, 7, 1, 8, 0, 8, 1, 2};
    wizard.tintShirt = wizard.tintPants = true;
    wizard.shirtTint = {46, 34, 70};
    wizard.pantsTint = {34, 28, 52};
    wizard.weapon = Item::Staff;
    wizard.helm = Item::WizardHat;
    SpeciesInfo barb = Make("barbarian", "Barbarian", 26, 40, 25, 26, 20, 1.9f, 90.0f, "A wild man from the highlands.", "barbarian", Rig::Human, 1.9f);
    barb.aggressive = true;
    barb.aggroRange = 4;
    barb.look = {false, 4, 1, 3, 0, 6, 0, 6, 1, 3};
    barb.tintShirt = true;
    barb.shirtTint = {116, 84, 54};
    barb.tintPants = true;
    barb.pantsTint = {84, 66, 44};
    barb.weapon = GearItem(Tier::Iron, GearType::Axe);
    SpeciesInfo ogre = Make("ogre", "Ogre", 38, 70, 36, 38, 32, 1.5f, 120.0f, "A hulking ogre with a big club.", "ogre", Rig::Human, 2.7f);
    ogre.aggressive = true;
    ogre.aggroRange = 3;
    ogre.scale = 1.5f;
    ogre.look = {false, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ogre.tintSkin = true;
    ogre.skinTint = {150, 134, 88};
    ogre.tintShirt = true;
    ogre.shirtTint = {96, 66, 40};
    ogre.tintPants = true;
    ogre.pantsTint = {70, 52, 34};
    ogre.weapon = GearItem(Tier::Steel, GearType::Mace);
    SpeciesInfo skw = Make("skeleton_warrior", "Skeleton warrior", 34, 46, 33, 32, 30, 1.9f, 100.0f, "A skeleton in rusty armour. It remembers how to fight.", "skeleton_warrior", Rig::Human, 1.8f);
    skw.aggressive = true;
    skw.aggroRange = 4;
    skw.look = {false, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    skw.tintSkin = skw.bald = true;
    skw.skinTint = {226, 220, 200};
    skw.tintShirt = skw.tintPants = true;
    skw.shirtTint = {204, 198, 178};
    skw.pantsTint = {190, 184, 164};
    skw.weapon = GearItem(Tier::Steel, GearType::Sword);
    skw.shield = GearItem(Tier::Steel, GearType::Shield);
    skw.helm = GearItem(Tier::Iron, GearType::Helm);
    SpeciesInfo knight = Make("black_knight", "Black knight", 55, 88, 55, 56, 54, 1.9f, 200.0f, "A knight in dark armour. He serves no king.", "black_knight", Rig::Human, 1.9f);
    knight.aggressive = true;
    knight.aggroRange = 5;
    knight.look = {false, 3, 3, 1, 0, 9, 0, 13, 1, 2};
    knight.weapon = GearItem(Tier::Steel, GearType::Sword);
    knight.shield = GearItem(Tier::Iron, GearType::Shield);
    knight.helm = GearItem(Tier::Iron, GearType::Helm);
    knight.body = GearItem(Tier::Iron, GearType::Body);
    knight.legs = GearItem(Tier::Iron, GearType::Legs);
    SpeciesInfo giant = Make("hill_giant", "Hill giant", 44, 85, 42, 44, 38, 1.6f, 140.0f, "A giant of the highlands, grumpy and hungry.", "hill_giant", Rig::Human, 3.2f);
    giant.aggressive = true;
    giant.aggroRange = 3;
    giant.scale = 1.75f;
    giant.look = {false, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    giant.tintSkin = true;
    giant.skinTint = {170, 148, 122};
    giant.tintShirt = true;
    giant.shirtTint = {88, 100, 70};
    giant.tintPants = true;
    giant.pantsTint = {96, 76, 52};
    giant.weapon = GearItem(Tier::Iron, GearType::Mace);
    for (const SpeciesInfo& s : {chicken, cow, calf, goblin, warrior, rat, wolf, dark, spider, zombie, skeleton, troll, mummy, dragon, aw, aa, am, ac,
                                 cave, scorpion, jackal, hound, mole, hobgoblin, imp, baby, black, bandit, wizard, barb, ogre, skw, knight, giant})
        v.push_back(s);
    return v;
}

const std::vector<SpeciesInfo>& Species_Table() {
    static const std::vector<SpeciesInfo> table = BuildSpecies();
    return table;
}

int Chebyshev(TileCoord a, TileCoord b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
}

int RandInt(const std::function<double()>& roll, int lo, int hi) {   // inclusive
    return lo + std::min(hi - lo, static_cast<int>(roll() * (hi - lo + 1)));
}

// Keeps only the part of a path that stays inside the pen.
void TrimToPen(std::vector<TileCoord>* path, const TileRect& pen) {
    size_t keep = 0;
    while (keep < path->size() && pen.Contains((*path)[keep].x, (*path)[keep].z)) ++keep;
    path->resize(keep);
}

}  // namespace

const SpeciesInfo& GetSpecies(Species s) {
    const int i = static_cast<int>(s);
    return Species_Table()[(i >= 0 && i < kSpeciesCount) ? i : 0];
}

bool SpeciesFromKey(const std::string& key, Species* out) {
    for (int i = 0; i < kSpeciesCount; ++i)
        if (key == Species_Table()[i].key) {
            *out = static_cast<Species>(i);
            return true;
        }
    return false;
}

std::vector<ItemStack> RollLoot(Species s, const std::function<double()>& roll) {
    const LootTable* table = FindLootTable(GetSpecies(s).lootKey);
    return table ? RollLootTable(*table, roll) : std::vector<ItemStack>{};
}

// ---------------------------------------------------------------------------------------------------------------

void Npcs::Init(const TileMap& map) {
    npcs_.clear();
    int id = 1;
    for (const NpcDef& d : map.Npcs()) {
        Species sp;
        const PenDef* pen = map.FindPen(d.pen);
        if (!SpeciesFromKey(d.species, &sp) || !pen) continue;
        Npc n;
        n.id = id++;
        n.species = sp;
        n.colorway = GetSpecies(sp).colorway >= 0 ? GetSpecies(sp).colorway : n.id % 2;
        n.pen = pen->rect;
        n.homeX = n.x = d.x;
        n.homeZ = n.z = d.z;
        n.facing = (n.id * 2.399f);   // a different heading each
        n.maxHp = n.hp = GetSpecies(sp).hitpoints;
        n.idleLeft = 0.5f + (n.id % 5) * 0.7f;
        npcs_.push_back(n);
    }
}

Npc* Npcs::Find(int id) {
    for (Npc& n : npcs_)
        if (n.id == id) return &n;
    return nullptr;
}

const Npc* Npcs::Find(int id) const {
    for (const Npc& n : npcs_)
        if (n.id == id) return &n;
    return nullptr;
}

int Npcs::AliveAt(TileCoord t) const {
    for (const Npc& n : npcs_)
        if (n.Alive() && n.Tile() == t) return n.id;
    return -1;
}

bool Npcs::Hurt(int id, int damage) {
    Npc* n = Find(id);
    if (!n || n->dead) return false;
    n->hurtT = 0.0f;
    n->hp = std::max(0, n->hp - std::max(0, damage));
    if (n->hp > 0) return false;
    n->dead = true;
    n->lootPending = true;
    n->deathT = 0.0f;
    n->hunting = false;
    n->path.clear();
    n->respawnLeft = n->Info().respawnSeconds;
    return true;
}

void Npcs::Engage(int id, int who, float x, float z) {
    Npc* n = Find(id);
    if (!n || n->dead || n->hunting) return;
    n->engagedBy = who;
    n->engageLeft = 1.0f;
    n->path.clear();
    n->idleLeft = 1.5f;
    const float dx = x - n->x, dz = z - n->z;
    if (dx * dx + dz * dz > 1e-6f) n->facing = std::atan2(dx, dz);
}

bool Npcs::Engaged(int id) const {
    const Npc* n = Find(id);
    return n && n->engagedBy != 0;
}

void Npcs::Alert(const Npc& src) {
    const int radius = src.Info().pack;
    if (radius <= 0) return;
    for (Npc& o : npcs_) {
        if (o.id == src.id || o.dead || o.species != src.species || o.hunting) continue;
        if (Chebyshev(o.Tile(), src.Tile()) > radius) continue;
        o.hunting = true;
        o.huntLeft = kHuntSeconds;
        o.idleLeft = 0.0f;
    }
}

void Npcs::StartHunt(int id) {
    if (Npc* n = Find(id)) {
        n->hunting = true;
        n->huntLeft = kHuntSeconds;
        n->idleLeft = 0.0f;
        Alert(*n);
    }
}

void Npcs::RefreshHunt(int id) {
    if (Npc* n = Find(id))
        if (n->hunting) n->huntLeft = kHuntSeconds;
}

void Npcs::StopAllHunts() {
    for (Npc& n : npcs_) n.hunting = false;
}

bool Npcs::Occupied(TileCoord t, int exceptId, TileCoord player) const {
    if (t == player) return true;
    for (const Npc& n : npcs_) {
        if (n.id == exceptId || n.dead) continue;
        if (n.Tile() == t) return true;
        if (!n.path.empty() && n.path.back() == t) return true;
    }
    return false;
}

void Npcs::ChaseTile(int id, TileCoord t, float seconds) {
    Npc* n = Find(id);
    if (!n || n->dead) return;
    n->chaseTile = t;
    n->chaseLeft = seconds;
}

void Npcs::Move(Npc& n, float dt) {
    float budget = n.Info().speed * dt;
    while (budget > 0.0f && !n.path.empty()) {
        const float tx = n.path.front().x + 0.5f, tz = n.path.front().z + 0.5f;
        const float dx = tx - n.x, dz = tz - n.z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 1e-4f) n.facing = std::atan2(dx, dz);
        if (dist <= budget) {
            n.x = tx;
            n.z = tz;
            budget -= dist;
            n.path.pop_front();
        } else {
            n.x += dx / dist * budget;
            n.z += dz / dist * budget;
            budget = 0.0f;
        }
    }
    if (!n.path.empty()) n.walkPhase += dt * n.Info().speed * 3.4f;
}

void Npcs::Wander(Npc& n, const TileMap& map, TileCoord player, const std::function<double()>& roll) {
    // Somewhere within 3 tiles that is inside the pen, open, and not already taken.
    for (int tries = 0; tries < 12; ++tries) {
        const TileCoord here = n.Tile();
        const TileCoord t{here.x + RandInt(roll, -3, 3), here.z + RandInt(roll, -3, 3)};
        if (t == here || !n.pen.Contains(t.x, t.z) || !map.Walkable(t) || Occupied(t, n.id, player)) continue;
        PathResult p = FindPath(map, here, t);
        if (!p.found) continue;
        TrimToPen(&p.tiles, n.pen);
        if (p.tiles.empty()) continue;
        n.path.assign(p.tiles.begin(), p.tiles.end());
        break;
    }
    // Chickens dart about; cows stand and chew for longer.
    const bool quick = n.Info().speed >= 1.6f;
    const float base = quick ? 1.2f : 2.5f;
    n.idleLeft = base + static_cast<float>(roll()) * (quick ? 3.5f : 5.5f);
    if (n.path.empty()) n.facing += static_cast<float>(roll() - 0.5) * 2.0f;   // just turn on the spot
}

void Npcs::Chase(Npc& n, const TileMap& map, TileCoord player, const std::function<double()>& roll) {
    (void)roll;
    const TileCoord here = n.Tile();
    const SpeciesInfo& info = n.Info();
    if (info.attackRange > 1 && Chebyshev(here, player) <= info.attackRange && map.ProjectileClear(here, player)) {   // a shooter: stand off and fire
        n.path.clear();
        n.facing = std::atan2(player.x + 0.5f - n.x, player.z + 0.5f - n.z);
        return;
    }
    if (Chebyshev(here, player) <= 1) {   // next to the hero: stand and fight
        n.path.clear();
        n.facing = std::atan2(player.x + 0.5f - n.x, player.z + 0.5f - n.z);
        return;
    }
    if (n.repathLeft > 0.0f && !n.path.empty()) return;
    n.repathLeft = 0.4f;
    // Walk to the free tile next to the hero that is nearest to us.
    bool found = false;
    PathResult best;
    int bestLen = 1 << 20;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            const TileCoord t{player.x + dx, player.z + dz};
            if (t == player || !n.pen.Contains(t.x, t.z) || !map.Walkable(t) || Occupied(t, n.id, player)) continue;
            PathResult p = FindPath(map, here, t);
            if (!p.found) continue;
            TrimToPen(&p.tiles, n.pen);
            if (p.tiles.empty() || static_cast<int>(p.tiles.size()) >= bestLen) continue;
            bestLen = static_cast<int>(p.tiles.size());
            best = p;
            found = true;
        }
    if (found) n.path.assign(best.tiles.begin(), best.tiles.end());
}

void Npcs::Respawn(Npc& n, const TileMap& map, TileCoord player, const std::function<double()>& roll) {
    TileCoord t{static_cast<int>(n.homeX), static_cast<int>(n.homeZ)};
    for (int tries = 0; tries < 40 && (Occupied(t, n.id, player) || !map.Walkable(t)); ++tries)
        t = {RandInt(roll, n.pen.x0, n.pen.x1 - 1), RandInt(roll, n.pen.z0, n.pen.z1 - 1)};
    n.x = t.x + 0.5f;
    n.z = t.z + 0.5f;
    n.hp = n.maxHp;
    n.dead = false;
    n.lootPending = false;
    n.hunting = false;
    n.path.clear();
    n.spawnT = 0.0f;
    n.idleLeft = 1.0f + static_cast<float>(roll()) * 3.0f;
}

void Npcs::Update(float dt, const TileMap& map, TileCoord player, const std::function<double()>& roll) {
    for (Npc& n : npcs_) {
        if (n.dead) {
            n.deathT += dt;
            n.respawnLeft -= dt;
            if (n.respawnLeft <= 0.0f && n.deathT > kCorpseSeconds) Respawn(n, map, player, roll);
            continue;
        }
        n.spawnT += dt;
        n.attackT += dt;
        n.hurtT += dt;
        n.repathLeft -= dt;
        if (n.chaseLeft > 0.0f) n.chaseLeft -= dt;
        if (n.hp < n.maxHp) {   // mending: a hit point a minute, like everybody
            n.regenT += dt;
            if (n.regenT >= kRegenSeconds) {
                n.regenT -= kRegenSeconds;
                ++n.hp;
            }
        } else {
            n.regenT = 0.0f;
        }
        if (n.hunting) {
            n.huntLeft -= dt;
            if (n.huntLeft <= 0.0f) n.hunting = false;
        }
        // Aggressive creatures pick a fight with a hero who comes close - unless he is far too strong to bother with.
        const SpeciesInfo& info = n.Info();
        if (info.aggressive && !n.hunting && heroAlive_ && n.spawnT > 1.0f && heroCombat_ < info.level * 2 + 5 &&
            Chebyshev(n.Tile(), player) <= info.aggroRange) {
            n.hunting = true;
            n.huntLeft = kHuntSeconds;
            n.idleLeft = 0.0f;
            Alert(n);
        }
        if (n.engagedBy != 0) {   // somebody else is fighting it: stand and face them (until the hero joins in, or they leave)
            n.engageLeft -= dt;
            if (n.engageLeft <= 0.0f || n.hunting) n.engagedBy = 0;
        }
        if (n.engagedBy != 0 && !n.hunting && n.chaseLeft <= 0.0f) {
            n.path.clear();
            continue;
        }
        if (n.hunting) {
            Chase(n, map, player, roll);
        } else if (n.chaseLeft > 0.0f) {   // going for an adventurer
            Chase(n, map, n.chaseTile, roll);
        } else {
            n.idleLeft -= dt;
            if (n.path.empty() && n.idleLeft <= 0.0f) Wander(n, map, player, roll);
        }
        Move(n, dt);
    }
}

}  // namespace journey
