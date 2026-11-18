// What the hero can DO in the world: chop trees, net fish, mine rocks, light fires, cook, smelt and smith, eat, bury bones,
// pick things up, wear gear, talk to people, buy and sell, and fight creatures. Pure C++ (unit tested).
//
// main.cpp turns taps into calls here (TapTree, TapNpc, ...) and feeds it time; this class walks the hero to the
// target, runs the right timer, and changes the bag / skills / world. It talks back through GameEvents so it never
// needs to know about the screen. It also builds the context menus (BuildMenu) and carries out the chosen entry.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "game/Combat.h"
#include "game/Cooking.h"
#include "game/Equipment.h"
#include "game/Fires.h"
#include "game/Fishing.h"
#include "game/Forest.h"
#include "game/GroundItems.h"
#include "game/Inventory.h"
#include "game/Menu.h"
#include "game/Mining.h"
#include "game/Npc.h"
#include "game/People.h"
#include "game/Player.h"
#include "game/Shops.h"
#include "game/Skills.h"
#include "game/Smithing.h"
#include "game/Woodcutting.h"
#include "world/TileMap.h"

namespace journey {

// Colour of a chat line: Normal = white, Info = yellow hint, Warn = red, Good = gold (level-ups).
enum class Tone { Normal, Info, Warn, Good };

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
};

// What the hero is busy doing right now (drives the animation and the tool in his hand).
enum class Activity { None, Chopping, Fishing, Lighting, Cooking, Burying, Fighting, Mining, Smelting, Smithing };

class Gameplay {
public:
    Gameplay(TileMap& map, Player& player, Skills& skills, Inventory& bag, Equipment& equip, Forest& forest, Rocks& rocks,
             Fires& fires, Npcs& npcs, GroundItems& ground, Shops& shops, People& people, GameEvents& events, std::function<double()> roll)
        : map_(map), player_(player), skills_(skills), bag_(bag), equip_(equip), forest_(forest), rocks_(rocks), fires_(fires),
          npcs_(npcs), ground_(ground), shops_(shops), people_(people), events_(events), roll_(std::move(roll)) {}

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
    void TapPile(int pileId);            // walk onto it and take everything that fits
    void TakeFromPile(int pileId, int itemIndex);
    // A button under the selected bag slot (or an entry of the item's menu).
    void DoItemAction(int slot, ItemAction action);
    // "Use" an item: the next tap on another bag item or on a fire is the thing to use it with.
    void BeginUse(int slot);
    bool UseItemOnSlot(int slot);        // true if a use was pending (and is now done or refused)
    void UseItemOnFire(int slot, int fireId);
    void UseItemOnStation(int slot, int stationIndex);
    int UseSlot() const { return useSlot_; }
    // The player walked somewhere else (or tapped empty ground): drop whatever we were doing.
    void Cancel();

    // ---- smelting and smithing (the hero must be standing next to the furnace / anvil) ----
    MakeMenu BuildSmeltMenu() const;
    MakeMenu BuildSmithMenu(Tier tier) const;
    // `count` = how many to make (it stops sooner if the ore or bars run out).
    void StartSmelt(Tier tier, int count);
    void StartSmith(Tier tier, GearType type, int count);

    // ---- shops ----
    ShopMenu BuildShopMenu(int shop) const;
    void Buy(int shop, int line, int qty);       // `line` = the shelf line (ShopLine::index)
    void Sell(int shop, Item item, int qty);
    int Coins() const { return bag_.Count(Item::Coins); }

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

    Activity Current() const;
    // True once after a Tap*() that accepted the target: where to flash the marker (red = interact, yellow = walk).
    bool TakeMarker(float* x, float* z, bool* red = nullptr);

private:
    enum class Goal { None, Chop, Fish, Cook, Light, Bury, Attack, Take, Mine, Station, Smelt, Smith, Person };
    struct PendingHit {
        float timer;
        bool onPlayer;
        int npcId;
        int damage;
    };

    void Approach(TileCoord target);   // walk next to a (blocked) target tile, then Arrived()
    void Arrived();
    void StartLighting();
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
    GameEvents& events_;
    std::function<double()> roll_;

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
    float makeTimer_ = 0.0f;
    LightSession light_;
    float cookTimer_ = 0.0f;
    float buryTimer_ = 0.0f;
    Item buryItem_ = Item::None;
    float repathLeft_ = 0.0f;
    int useSlot_ = -1;

    // combat
    CombatStyle style_ = CombatStyle::Accurate;
    float turnTimer_ = 0.0f;
    bool playerTurn_ = true;
    float blockAge_ = 99.0f;
    bool blockLanded_ = false;
    float strikeAge_ = 99.0f;
    bool strikeLeft_ = false;
    float regenTimer_ = 0.0f;
    double hpXpCarry_ = 0.0;   // the fractions of Hitpoints XP (1.33 per damage) that have not added up to a whole point yet
    std::vector<PendingHit> pending_;

    bool markerPending_ = false;
    bool markerRed_ = true;
    float markerX_ = 0, markerZ_ = 0;
};

}  // namespace journey
