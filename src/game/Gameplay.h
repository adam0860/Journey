// What the hero can DO in the world: chop trees, net fish, mine rocks, light fires, cook, smelt and smith, eat, bury bones,
// pick things up, wear gear, talk to people, buy and sell, and fight creatures. Pure C++ (unit tested).
//
// main.cpp turns taps into calls here (TapTree, TapNpc, ...) and feeds it time; this class walks the hero to the
// target, runs the right timer, and changes the bag / skills / world. It talks back through GameEvents so it never
// needs to know about the screen. It also builds the context menus (BuildMenu) and carries out the chosen entry.
#pragma once
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "game/Archery.h"
#include "game/Bank.h"
#include "game/Combat.h"
#include "game/Crafting.h"
#include "game/Cooking.h"
#include "game/Equipment.h"
#include "game/Fires.h"
#include "game/Fishing.h"
#include "game/Foes.h"
#include "game/Forest.h"
#include "game/GroundItems.h"
#include "game/Inventory.h"
#include "game/Magic.h"
#include "game/Menu.h"
#include "game/Mining.h"
#include "game/Npc.h"
#include "game/People.h"
#include "game/Player.h"
#include "game/Quests.h"
#include "game/Shops.h"
#include "game/Skilling.h"
#include "game/Skills.h"
#include "game/Smithing.h"
#include "game/Woodcutting.h"
#include "world/Pathfinder.h"
#include "world/TileMap.h"

namespace journey {

// Colour of a chat line: Normal = white, Info = yellow hint, Warn = red, Good = gold (level-ups).
enum class Tone { Normal, Info, Warn, Good };

bool IsDangerousArea(const std::string& areaName);

// An arrow or a spell leaving the hero's hand (the screen draws it flying). It leaves `delay` seconds after the strike starts
// and takes `flight` seconds to arrive, which is when the hit lands.
struct ShotFx {
    bool magic = false;
    float fromX = 0, fromZ = 0, toX = 0, toZ = 0;
    float delay = 0.18f, flight = 0.2f;
    Rgb color = {255, 255, 255};   // spells: the glow
    int metal = 0;                 // arrows: 0 bronze .. 3 mithril (the head)
    int npcId = -1;                // the target (hero shots) or the shooter (monster shots)
    bool toHero = false;           // a monster shooting at the hero (npcId is the shooter)
};

class GameEvents {
public:
    virtual ~GameEvents() = default;
    virtual void Say(const std::string& text, Tone tone) = 0;
    virtual void XpDrop(Skill skill, int amount) = 0;
    virtual void LevelUp(Skill skill, int newLevel) = 0;
    // A strike landed (damage > 0, red) or missed (damage == 0, blue). onPlayer: the hero was hit; else npcId was.
    virtual void Hit(bool onPlayer, int npcId, int damage) = 0;
    virtual void PlayerDied() = 0;   // he has already been put back at the spawn point with full hit points
    virtual void Changed() = 0;      // bag/skills/gear changed: a good moment to save
    // The hero reached a furnace or an anvil: show what he can make there.
    virtual void OpenMake(const MakeMenu& menu) = 0;
    // The hero reached a shopkeeper: show the shop window.
    virtual void OpenShop(const ShopMenu& menu) = 0;
    // The hero reached a banker: show the bank window.
    virtual void OpenBank() = 0;
    // An arrow or spell was shot / cast.
    virtual void Shoot(const ShotFx& fx) = 0;
};

// What the hero is busy doing right now (drives the animation and the tool in his hand).
enum class Activity { None, Chopping, Fishing, Lighting, Cooking, Burying, Fighting, Mining, Smelting, Smithing, Crafting };

class Gameplay {
public:
    Gameplay(TileMap& map, Player& player, Skills& skills, Inventory& bag, Equipment& equip, Forest& forest, Rocks& rocks,
             Fires& fires, Npcs& npcs, GroundItems& ground, Shops& shops, People& people, Bank& bank, GameEvents& events,
             std::function<double()> roll)
        : map_(map), player_(player), skills_(skills), bag_(bag), equip_(equip), forest_(forest), rocks_(rocks), fires_(fires),
          npcs_(npcs), ground_(ground), shops_(shops), people_(people), bank_(bank), events_(events), roll_(std::move(roll)) {}

    // ---- things the player taps (each cancels what he was doing) ----
    void WalkTo(float gx, float gz);
    void TapTree(int treeIndex);
    void TapSpot(int spotIndex);
    void TapRock(int rockIndex);
    void TapStation(int stationIndex);   // walk to a furnace / anvil and open its make menu
    void TapFire(int fireId);
    // Walk up to a person and talk (opening their shop, if they run one, unless talkOnly).
    void TapPerson(int personId, bool talkOnly = false);
    void TapNpc(int npcId);              // attack it
    void TapFoe(int foeId);              // attack another adventurer (the Abyss only; this may put a skull over his head)
    // The skull (see Foes.h): ten minutes after he attacks a player who is not skulled. Skulled, a death in the Abyss costs him everything.
    bool Skulled() const { return skullLeft_ > 0.0f; }
    float SkullLeft() const { return skullLeft_; }
    void SetSkull(float seconds) { skullLeft_ = seconds > 0.0f ? seconds : 0.0f; }
    // His record against other players: those he has killed, and the times a player has killed him (saved).
    int PvpKills() const { return pvpKills_; }
    int PvpDeaths() const { return pvpDeaths_; }
    void SetPvpRecord(int kills, int deaths) {
        pvpKills_ = kills > 0 ? kills : 0;
        pvpDeaths_ = deaths > 0 ? deaths : 0;
    }
    int AbyssLevel() const { return AbyssLevelAt(map_, player_.Tile().x, player_.Tile().z); }   // 0 outside the Abyss
    // The other adventurers (the Abyss fighters among them): where Gameplay finds them and hurts them.
    void SetFoes(FoeSource* foes) { foes_ = foes; }
    // An adventurer's blow lands on the hero (after the usual wind-up); the hero hits back if he was standing about.
    void StrikeHero(int foeId, int damage);
    void TapPile(int pileId);            // walk onto it and take everything that fits
    void TakeFromPile(int pileId, int itemIndex);
    // A button under the selected bag slot (or an entry of the item's menu).
    void DoItemAction(int slot, ItemAction action);
    // "Use" an item: the next tap on another bag item or on a fire is the thing to use it with.
    void BeginUse(int slot);
    bool UseItemOnSlot(int slot);        // true if a use was pending (and is now done or refused)
    void UseItemOnFire(int slot, int fireId);
    void UseItemOnStation(int slot, int stationIndex);
    void UseItemHere(int slot);          // use a bag item where he stands (a "use item" quest step)
    // The nearest walkable tile to the middle of a named place ({-1,-1} if there is none), and where a dead hero wakes up.
    TileCoord TileNearArea(const std::string& areaName) const;
    TileCoord RespawnTile(std::string* place = nullptr) const;
    int UseSlot() const { return useSlot_; }
    // The player walked somewhere else (or tapped empty ground): drop whatever we were doing.
    void Cancel();

    // ---- smelting and smithing (the hero must be standing next to the furnace / anvil) ----
    MakeMenu BuildSmeltMenu() const;
    MakeMenu BuildSmithMenu(Tier tier) const;
    // `count` = how many to make (it stops sooner if the ore or bars run out).
    void StartSmelt(Tier tier, int count);
    void StartSmith(Tier tier, GearType type, int count);

    // ---- crafting: the menu of one kind of crafting, and making things (a station kind needs the hero next to it) ----
    MakeMenu BuildCraftMenu(CraftKind kind) const;
    bool ShopLineUnlocked(int shop, Item item) const;   // false while a quest-gated shelf is still shut to this hero
    void StartCraft(int recipeIndex, int count);

    // ---- shops ----
    ShopMenu BuildShopMenu(int shop) const;
    void Buy(int shop, int line, int qty);       // `line` = the shelf line (ShopLine::index)
    void Sell(int shop, Item item, int qty);
    int Coins() const { return bag_.Count(Item::Coins); }

    // ---- conversations and quests ----
    // What the dialogue box shows right now (open = false when nobody is talking). A page with no options is "tap to continue".
    DialogueView Dialogue() const;
    void DialogueChoose(int option);   // the player tapped option `option` of this page (or -1 for "continue")
    void DialogueClose();              // walked away: the conversation just ends
    // The map markers: the next step of every quest in progress, and the people who could give him a quest right now.
    std::vector<QuestMarker> QuestMarkers() const;
    QuestLog& Quests() { return quests_; }
    const QuestLog& Quests() const { return quests_; }
    // The named places (map @area names) the hero has been to. Saved; a teleport scroll only works for a place he has seen.
    const std::set<std::string>& Visited() const { return visited_; }
    void SetVisited(const std::set<std::string>& v) { visited_ = v; }
    void MarkVisited(const std::string& place) { visited_.insert(place); }

    // ---- the bank (the hero must be within reach of a banker) ----
    bool NearBanker() const;
    void BankDeposit(Item item, int qty);   // from the bag into the bank
    void BankDepositAll();                  // everything in the bag
    void BankWithdraw(Item item, int qty);  // from the bank into the bag
    const Bank& GetBank() const { return bank_; }

    // ---- worn gear ----
    void Unequip(EquipSlot slot);
    std::vector<MenuEntry> BuildEquipMenu(EquipSlot slot) const;
    const Equipment& Worn() const { return equip_; }

    // ---- context menus ----
    // `hits` = what is under the finger, nearest first. gx/gz = the ground point (hasGround false for the sky).
    std::vector<MenuEntry> BuildMenu(const std::vector<Target>& hits, bool hasGround, float gx, float gz) const;
    // The menu of a bag item (long-press or right-click in the Bag).
    std::vector<MenuEntry> BuildItemMenu(int slot) const;
    void Perform(const MenuEntry& entry);
    void Examine(Target t);

    void Update(float dt);

    // ---- combat settings / state ----
    CombatStyle Style() const { return style_; }
    void SetStyle(CombatStyle s) { style_ = s; }
    // How he attacks right now (a bow shoots, a staff casts, anything else is melee), how far that reaches, and the spell a staff casts.
    AttackKind Kind() const;
    // How many ticks (0.6 s) and seconds one swing of what he wields takes (3 for a dagger ... 6 for a warhammer).
    int HeroSwingTicks() const;
    float HeroSwingSeconds() const;
    Fighter HeroFighter() const { return PlayerNow(); }      // what an adventurer's blow is rolled against
    int HeroMaxHit() const { return MaxHit(PlayerNow()); }   // the biggest melee hit he can land with what he wields now
    int AttackReach() const;
    int SpellIndex() const { return spell_; }
    void SetSpell(int index);
    // Seconds since the hero last threw a punch (large when he has not), and which fist.
    float StrikeAge() const { return strikeAge_; }
    bool StrikeLeft() const { return strikeLeft_; }
    // The animal he is fighting, or -1.
    int FightTarget() const { return goal_ == Goal::Attack && working_ ? target_ : -1; }
    // Seconds since a creature last struck at him, and whether that strike lands (he flinches) or is blocked.
    float BlockAge() const { return blockAge_; }
    bool BlockLanded() const { return blockLanded_; }
    // Mining / smelting / smithing: where he is working (the rock or the station) and how far through a swing / blow he is
    // (0..1; the pickaxe or hammer lands where the phase wraps round from 1 to 0).
    bool WorkSpot(float* x, float* z) const;
    float WorkPhase() const;
    // The pickaxe he swings (None if he has none he can use).
    Item PickaxeInUse() const { return BestPickaxe(bag_, skills_.Level(Skill::Mining)); }
    // The axe he chops with (None if he has none he can use).
    Item AxeInUse() const { return BestHatchet(bag_, skills_.Level(Skill::Woodcutting)); }

    // Poison (from a spider's bite): damage every few seconds, weakening until it wears off.
    bool Poisoned() const { return poisonDamage_ > 0; }

    Activity Current() const;
    // True once after a Tap*() that accepted the target: where to flash the marker (red = interact, yellow = walk).
    bool TakeMarker(float* x, float* z, bool* red = nullptr);

private:
    enum class Goal { None, Chop, Fish, Cook, Light, Bury, Attack, Take, Mine, Station, Smelt, Smith, Person, Craft };
    struct PendingHit {
        float timer;
        bool onPlayer;
        int npcId;
        int damage;
        AttackKind kind = AttackKind::Melee;   // how the hero's strike was made (decides the XP)
        Item arrow = Item::None;               // an arrow that will land on the ground where it hits (or misses)
    };

    void Approach(TileCoord target);   // walk next to a (blocked) target tile, then Arrived()
    void Arrived();
    void StartLighting(bool bonfire = false, Item bonfireLog = Item::None);
    void UpdateMake(float dt);
    int NeedDistance() const;                    // how close to the target the hero has to get (0 = on it, 1 = next to it, more = across a counter)
    bool NearShopKeeper(int shop) const;
    int NearbyStation(const char* kind) const;   // index of a station of this kind next to the hero, or -1
    Fighter PlayerNow() const;                   // the hero's combat numbers: levels + style + worn gear
    void StartBury(int slot);
    void UpdateWork(float dt);
    void UpdateCombat(float dt);
    void PlayerTurn();
    void CreatureTurn();
    void ApplyHit(const PendingHit& h);
    void UpdateLoot();
    void PlayerDies();
    void GainXp(Skill skill, int amount);
    void Say(const std::string& text, Tone tone = Tone::Normal) { events_.Say(text, tone); }
    void StepAwayFromFire(TileCoord fireTile);
    void TakeNow(int pileId, int itemIndex);
    TileCoord TargetTile() const;
    bool FightingSomething() const;
    void StartFight(int npcId, bool fromAutoRetaliate);
    Fighter NpcFighter(const Npc& n) const;
    bool InAttackRange(const Npc& n) const;        // close enough (and, for a bow or a spell, a clear line) to strike it
    bool InRangeOf(TileCoord t) const;             // the same for a tile
    bool TargetAt(int id, float* x, float* z) const;   // a living creature or foe by id: where it stands
    Fighter DefenderOf(int id) const;              // its numbers as the one being hit (a creature or a foe)
    bool MayFight(const FoeView& v, bool say);     // is that player within the gap of levels this deep in the Abyss?
    bool Provoked(int foeId) const;                // did this foe attack the hero in the last half minute? (then hitting back earns no skull)
    void ApplyFoeHit(const PendingHit& h);
    void AwardHitXp(AttackKind kind, int damage);
    // Where to walk to shoot or cast at a target: the nearest reachable tile within reach that has a clear line to it.
    PathResult ShootingPath(TileCoord target) const;
    bool WeaponReady(bool say);                    // has arrows / runes / levels for the weapon in his hand
    void StrikeAtRange(int id, float x, float z, AttackKind kind);
    struct DlgOption {
        std::string text;
        int act;   // 0 = next page, 1 = accept quest `arg`, 2 = end the conversation, 3 = take ferry route `arg`
        int arg;
    };
    struct DlgPage {
        std::string speaker, text;
        std::vector<DlgOption> options;
    };
    void Talk(Person* p, bool talkOnly);
    void Ferry(int route);                          // pay the ferryman and sail to the other dock
    void DropOnDeath(int keep);                     // dying in a danger zone: all but the `keep` most valuable items fall to the ground (0 when skulled)
    void ReadScroll(int bagSlot);                   // a teleport scroll: one is used up and the hero is whisked to its place
    void GiveQuestReward(int quest);
    void QuestProgressMessage(int quest);
    void EndDialogue();
    bool CreatureCanStrike(const Npc& n) const;    // is it close enough (or has it a clear shot) to hit the hero?
    void Poison(int damage);

    TileMap& map_;
    Player& player_;
    Skills& skills_;
    Inventory& bag_;
    Equipment& equip_;
    Forest& forest_;
    Rocks& rocks_;
    Fires& fires_;
    Npcs& npcs_;
    GroundItems& ground_;
    Shops& shops_;
    People& people_;
    Bank& bank_;
    GameEvents& events_;
    std::function<double()> roll_;
    FoeSource* foes_ = nullptr;
    int pvpKills_ = 0, pvpDeaths_ = 0;
    float skullLeft_ = 0.0f;           // seconds of skull left
    float foeHitLeft_ = 0.0f;          // an adventurer struck at him this recently: he is in a fight
    std::vector<std::pair<int, float>> foeAggro_;   // foes that attacked him (id, seconds left of "they started it")

    Goal goal_ = Goal::None;
    bool walking_ = false;   // on the way to the target
    bool working_ = false;   // at the target, timer running
    int target_ = -1;        // tree index / spot index / fire id / npc id / pile id
    int targetSub_ = -1;     // pile item index (-1 = all) / bag slot
    ChopSession chop_;
    FishSession fish_;
    MineSession mine_;
    // smelting / smithing in progress
    Tier makeTier_ = Tier::Bronze;
    GearType makeType_ = GearType::Dagger;
    int makeLeft_ = 0;
    int craftRecipe_ = -1;
    float makeTimer_ = 0.0f;
    LightSession light_;
    Item lightLog_ = Item::None;   // the kind of log being lit
    bool lightBig_ = false;        // ...as a bonfire (four of them)
    float cookTimer_ = 0.0f;
    float buryTimer_ = 0.0f;
    Item buryItem_ = Item::None;
    float repathLeft_ = 0.0f;
    int useSlot_ = -1;

    // combat
    CombatStyle style_ = CombatStyle::Accurate;
    int spell_ = 0;            // index into AllSpells(): what a staff casts
    float turnTimer_ = 0.0f;       // seconds since the hero last swung
    float creatureTimer_ = 0.0f;   // seconds since the creatures last struck
    float blockAge_ = 99.0f;
    bool blockLanded_ = false;
    float strikeAge_ = 99.0f;
    bool strikeLeft_ = false;
    float regenTimer_ = 0.0f;
    QuestLog quests_;
    std::vector<DlgPage> pages_;
    size_t page_ = 0;
    int afterShop_ = -1;       // when the conversation ends: open this shop...
    bool afterBank_ = false;   // ...or the bank
    std::string lastArea_;
    std::set<std::string> visited_;   // named places the hero has stood in (saved; a teleport scroll needs its place to be here)
    int poisonDamage_ = 0;     // damage per poison tick (0 = not poisoned)
    int poisonTicks_ = 0;
    float poisonTimer_ = 0.0f;
    double hpXpCarry_ = 0.0;   // the fractions of Hitpoints XP (1.33 per damage) that have not added up to a whole point yet
    std::vector<PendingHit> pending_;

    bool markerPending_ = false;
    bool markerRed_ = true;
    float markerX_ = 0, markerZ_ = 0;
};

}  // namespace journey
