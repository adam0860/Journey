// Prototype HUD in a modern dark-glass style, laid out like the classic client:
// tab bar top-right, chat box bottom-left, area banner top-centre, hit points top-left.
// Tapping the Bag, Skills, Gear or Combat tab opens a panel under the tab bar (tap the tab again to close it).
// The Gear tab shows what he wears; the "make" panel (furnace / anvil) is a dialog in the middle of the screen.
// Also draws the combat feedback (hit splats, health bars over fighters) and the context menu.
#pragma once
#include <algorithm>
#include <deque>
#include <string>
#include <vector>

#include "game/Bank.h"
#include "game/Combat.h"
#include "game/Equipment.h"
#include "game/Inventory.h"
#include "game/Menu.h"
#include "game/People.h"
#include "game/Quests.h"
#include "game/Skills.h"
#include "raylib.h"
#include "world/TileMap.h"

namespace journey {

class Hud {
public:
    static constexpr int kTabBag = 0, kTabSkills = 1, kTabGear = 2, kTabQuests = 3, kTabMap = 4, kTabCombat = 5, kTabMagic = 6, kTabMenu = 7;

    // The HUD reads (never changes) the player's skills, bag, worn gear and bank. Call once at start-up.
    void Bind(const Skills* skills, const Inventory* bag, const Equipment* equip = nullptr, const Bank* bank = nullptr,
              const QuestLog* quests = nullptr) {
        skills_ = skills;
        bag_ = bag;
        equip_ = equip;
        bank_ = bank;
        quests_ = quests;
    }
    // ---- the dialogue box (a conversation): the game says what it shows, the HUD draws it and reports taps ----
    void SetDialogue(const DialogueView& d) { dialogue_ = d; }
    bool DialogueOpen() const { return dialogue_.open; }
    // The option under this point (0..n-1), kDialogueContinue for a tap on a page without options, -1 for a tap outside the box.
    static constexpr int kDialogueContinue = -2;
    int DialogueAt(Vector2 p) const;
    bool DialogueHits(Vector2 p) const;
    // ---- the Menu tab: change appearance, music volume, sound volume ----
    enum class SettingsHit { None, Appearance, MusicDown, MusicUp, SfxDown, SfxUp, Help, Back };   // (Help opens the how-to-play page in the Menu tab, Back returns)
    SettingsHit SettingsAt(Vector2 p) const;   // (None while the Menu tab is closed)
    void SetVolumes(float music, float sfx) {
        musicVol_ = music;
        sfxVol_ = sfx;
    }
    // ---- the world map (Map tab): the whole tile map as a little picture, built once; the people are drawn as dots ----
    void SetMap(const TileMap* map, const People* people);
    void SetPlayerTile(TileCoord t) { playerTile_ = t; }
    // Gold diamonds on the map: where the next step of each quest in progress is (and, hollow, who has a new quest to give him).
    void SetQuestMarkers(std::vector<QuestMarker> m) { markers_ = std::move(m); }
    // A panel with more in it than fits (the quest log, the how-to-play page) scrolls: drag it, or use the wheel over it.
    bool ScrollableAt(Vector2 p) const;
    void ScrollBy(float pixels) { scroll_ += pixels; }   // (kept inside the page when drawn)
    float ScrollMax() const { return scrollMax_; }
    float ScrollPos() const { return scroll_; }
    void SetCombatStyle(CombatStyle s) { combatStyle_ = s; }
    // How he attacks now (decides which style buttons the Combat tab offers) and the spell a staff casts (highlighted in the Magic tab).
    void SetAttackKind(AttackKind k) { attackKind_ = k; }
    void SetSpell(int i) { spell_ = i; }
    void SetPoisoned(bool p) { poisoned_ = p; }
    // Which spell row of the Magic tab is under this point, or -1 (also -1 while that tab is closed).
    int SpellAt(Vector2 p) const;
    // The spellbook shows kSpellsPerPage spells at a time: a tap on the arrows under them turns the page.
    static constexpr int kSpellsPerPage = 8;
    int SpellPageStep(Vector2 p) const;   // -1 (previous), +1 (next) or 0 when the tap is not on an arrow
    void StepSpellPage(int step);
    int SpellPage() const { return spellPage_; }

    void Say(const std::string& text, Color color = {255, 224, 120, 255});
    void ShowArea(const std::string& name);
    // Floating "+25 xp" text, like the classic client's experience drops.
    void XpDrop(Skill skill, int amount);
    void Update(float dt);
    void Draw(bool debug, const std::string& debugText) const;

    // ---- combat feedback ----
    // A hit splat over a fighter: red with the damage, or blue with a 0 for a miss. `world` = a point above its chest.
    void AddHitsplat(Vector3 world, int damage);
    // A health bar over a fighter this frame (fraction 0..1). Cleared every frame by DrawWorld().
    void QueueBar(Vector3 world, float fraction, bool hero);
    // A line of text over somebody's head this frame (a name, or what they say). Cleared every frame by DrawWorld().
    void QueueLabel(Vector3 world, const std::string& text, Color color, float size = 12.0f);
    // A little skull over somebody's head this frame (a skulled player). Cleared every frame by DrawWorld().
    void QueueSkull(Vector3 world);
    // The hero's own skull: seconds left (0 = none), shown under his health bar.
    void SetSkullTimer(float seconds) { skullTimer_ = seconds; }
    // The Abyss level where the hero stands (0 = not in the Abyss): shown under his health bar, with the gap of levels it allows.
    void SetAbyssLevel(int level) { abyssLevel_ = level; }
    // Draws the queued bars and the splats; call after EndMode3D() and before Draw().
    void DrawWorld(const Camera3D& camera);

    // ---- panels / taps ----
    // True if a screen point is over a HUD panel (so taps there do not walk the hero).
    bool Hits(Vector2 p) const;
    // Which tab (0..6) is at this point, or -1. The last one is "Menu".
    int TabAt(Vector2 p) const;
    // Open the tab's panel, or close it if it is already open. (The Menu tab has no panel.)
    void ToggleTab(int tab);
    void CloseTabs() { openTab_ = -1; }
    int OpenTab() const { return openTab_; }
    // Which bag slot (0..27) is under this point, or -1 (also -1 while the bag is closed).
    int BagSlotAt(Vector2 p) const;
    // The selected bag item shows buttons under the grid (Eat / Bury / Use / Drop / Examine...). -1 = nothing selected.
    void SelectSlot(int slot) { selected_ = slot; }
    int SelectedSlot() const { return selected_; }
    // The bag slot of the item being "used" (waiting for what to use it with), or -1. Drawn with a green frame.
    void SetUseSlot(int slot) { useSlot_ = slot; }
    // Which button of the selected item is under this point? Returns false if none.
    bool ActionAt(Vector2 p, ItemAction* action) const;
    // Which combat style button is under this point, or -1 (also -1 while the Combat tab is closed). Melee has four styles,
    // archery three (accurate / rapid / longrange), magic none.
    int StyleAt(Vector2 p) const;

    // Which worn-gear slot (EquipSlot as an int) is under this point, or -1 (also -1 while the Gear tab is closed).
    int EquipSlotAt(Vector2 p) const;

    // ---- the make panel (smelting at a furnace, smithing at an anvil) ----
    enum class MakeHitKind { None, Option, Tier, Qty, Close, Page };
    struct MakeHit {
        MakeHitKind kind = MakeHitKind::None;
        int index = 0;   // option index / tier / quantity (1, 5 or 99 = all) / page step (-1 or +1)
    };
    void OpenMake(MakeMenu menu) {
        // (the same menu coming back after a make keeps its page; a different one starts on the first)
        if (!makeOpen_ || make_.kind != menu.kind || make_.tier != menu.tier || make_.title != menu.title) makePage_ = 0;
        make_ = std::move(menu);
        makeOpen_ = true;
    }
    void CloseMake() { makeOpen_ = false; }
    void SetHelpPage(bool on) { helpPage_ = on; }
    // Lines for the bottom of the help page: "Players online" and the hiscores (the game fills them in every few seconds).
    void SetPlayerLines(std::vector<std::string> lines) { playerLines_ = std::move(lines); }
    bool MakeOpen() const { return makeOpen_; }
    const MakeMenu& Make() const { return make_; }
    int MakeQuantity() const { return makeQty_; }
    void SetMakeQuantity(int q) { makeQty_ = q; }
    void StepMakePage(int step);   // turn the page of a long make list (a tap on Prev / Next)
    MakeHit MakeAt(Vector2 p) const;   // what the point is on; kind None = on the panel but on nothing (or outside: check MakeHits)
    bool MakeHits(Vector2 p) const;

    // ---- the shop window (buy / sell tabs, a page of rows each) ----
    enum class ShopHitKind { None, Buy, Sell, TabBuy, TabSell, Qty, PagePrev, PageNext, Close };
    struct ShopHit {
        ShopHitKind kind = ShopHitKind::None;
        int index = 0;   // Buy / Sell: the row in the whole list;  Qty: 1, 5 or 99 (all)
    };
    void OpenShop(ShopMenu menu) {
        shop_ = std::move(menu);
        shopOpen_ = true;
        shopSell_ = false;
        shopPage_ = 0;
    }
    // After a purchase or sale: new numbers, same tab and page.
    void RefreshShop(ShopMenu menu) {
        shop_ = std::move(menu);
        shopPage_ = std::min(shopPage_, ShopPages() - 1);
    }
    void CloseShop() { shopOpen_ = false; }
    bool ShopOpen() const { return shopOpen_; }
    const ShopMenu& Shop() const { return shop_; }
    void SetShopTab(bool sell) {
        shopSell_ = sell;
        shopPage_ = 0;
    }
    void ShopPage(int delta) { shopPage_ = std::clamp(shopPage_ + delta, 0, ShopPages() - 1); }
    int ShopQuantity() const { return shopQty_; }
    void SetShopQuantity(int q) { shopQty_ = q; }
    bool ShopSellTab() const { return shopSell_; }
    ShopHit ShopAt(Vector2 p) const;
    bool ShopHits(Vector2 p) const;

    // ---- the bank window: the bank's items on the left (pages of 40), your bag on the right ----
    enum class BankHitKind { None, BankCell, BagCell, Qty, DepositAll, PagePrev, PageNext, Close };
    struct BankHit {
        BankHitKind kind = BankHitKind::None;
        int index = 0;   // BankCell: index into Bank::Slots();  BagCell: the bag slot;  Qty: 1, 5, 10 or Bank::kMaxStack (all)
    };
    void OpenBank() {
        bankOpen_ = true;
        bankPage_ = 0;
    }
    void CloseBank() { bankOpen_ = false; }
    bool BankOpen() const { return bankOpen_; }
    int BankQuantity() const { return bankQty_; }
    void SetBankQuantity(int q) { bankQty_ = q; }
    void BankPage(int delta) { bankPage_ = std::clamp(bankPage_ + delta, 0, BankPages() - 1); }
    BankHit BankAt(Vector2 p) const;
    bool BankHits(Vector2 p) const;

    // ---- context menu ----
    void OpenMenu(Vector2 at, std::vector<MenuEntry> entries);
    void CloseMenu() { menu_.clear(); }
    bool MenuOpen() const { return !menu_.empty(); }
    bool MenuHits(Vector2 p) const;
    // The entry under this point (index into the menu), or -1.
    int MenuEntryAt(Vector2 p) const;
    const MenuEntry& MenuEntryRef(int i) const { return menu_[i]; }
    // The line shown top-left for what a plain tap would do (desktop hover); empty runs hide it.
    void SetHover(std::vector<TextRun> runs, int moreOptions) {
        hover_ = std::move(runs);
        hoverMore_ = moreOptions;
    }
    // The mouse position, to highlight the menu row under it (desktop). (-1,-1) = none.
    void SetPointer(Vector2 p) { pointer_ = p; }

private:
    struct Line {
        std::string text;
        Color color;
    };
    struct Drop {
        std::string text;
        float age;
        int lane = 0;
    };
    struct Splat {
        Vector3 pos;
        int damage;
        float age;
    };
    struct Bar {
        Vector3 pos;
        float fraction;
        bool hero;
    };
    struct Label {
        Vector3 pos;
        std::string text;
        Color color;
        float size;
    };
    float Scale() const;
    Rectangle ChatRect() const;
    Rectangle TabRect(int i) const;
    Rectangle PanelRect() const;          // the open tab's panel (only valid while openTab_ >= 0)
    Rectangle BagCellRect(int slot) const;
    Rectangle ActionButtonRect(int index) const;   // buttons under the bag grid
    Rectangle StyleRect(int i) const;              // combat style buttons
    Rectangle GearSlotRect(int slot) const;        // the worn-gear slots on the Gear tab
    Rectangle MakeRectOf() const;                  // the make panel
    Rectangle MakeRowRect(int i) const;
    int MakeRowsPerPage() const;                   // how many rows fit on screen (the whole list when it all fits)
    int MakePages() const;
    Rectangle MakePageRect(bool next) const;       // the Prev / Next buttons under the rows
    Rectangle MakeTierRect(int t) const;
    Rectangle MakeQtyRect(int i) const;
    Rectangle MakeCloseRect() const;
    Rectangle ShopRectOf() const;
    Rectangle ShopRowRect(int i) const;
    Rectangle ShopTabRect(int t) const;
    Rectangle ShopQtyRect(int i) const;
    Rectangle ShopPageRect(int i) const;
    Rectangle ShopCloseRect() const;
    Rectangle BankRectOf() const;
    Rectangle BankCellRect(int i) const;        // a cell of the bank grid (i = position on the page)
    Rectangle BankBagCellRect(int slot) const;
    Rectangle BankQtyRect(int i) const;
    Rectangle BankDepositRect() const;
    Rectangle BankPageRect(int i) const;
    Rectangle BankCloseRect() const;
    Rectangle SpellRect(int i) const;
    Rectangle DialogueRectOf() const;
    Rectangle DialogueOptionRect(int i) const;
    std::vector<std::string> WrapText(const Font& font, const std::string& text, float size, float maxWidth) const;
    void DrawDialogue(float s, const Font& font) const;
    void DrawQuests(Rectangle panel, float s, const Font& font) const;
    void DrawMap(Rectangle panel, float s, const Font& font) const;
    void DrawSettings(Rectangle panel, float s, const Font& font) const;
    void DrawHelp(Rectangle panel, float s, const Font& font) const;
    Rectangle HelpBackRect() const;
    Rectangle SpellPageRect(bool next) const;
    int SpellPageCount() const;
    std::vector<std::string> HelpLines(const Font& font, float size, float maxWidth) const;
    Rectangle SettingsRow(int row) const;                  // 0 = appearance button, 1 = music row, 2 = sound row, 3 = how to play
    Rectangle SettingsButton(int row, bool plus) const;    // the - and + buttons of a volume row
    int BankPages() const;
    void DrawBank(float s, const Font& font) const;
    void DrawMagic(Rectangle panel, float s, const Font& font) const;
    void DrawAmmoPill(float s, const Font& font, float x, float y) const;
    int ShopPages() const;
    const std::vector<ShopLine>& ShopList() const { return shopSell_ ? shop_.sell : shop_.buy; }
    void DrawShop(float s, const Font& font) const;
    void DrawCoins(float s, const Font& font, float x, float y) const;
    void DrawGear(Rectangle panel, float s, const Font& font) const;
    void DrawMake(float s, const Font& font) const;
    Rectangle MenuRect() const;
    Rectangle MenuRowRect(int i) const;
    void DrawBag(Rectangle panel, float s, const Font& font) const;
    void DrawSkills(Rectangle panel, float s, const Font& font) const;
    void DrawCombat(Rectangle panel, float s, const Font& font) const;
    void DrawVitals(float s, const Font& font, float y) const;
    void DrawMenu(float s, const Font& font) const;
    float RunsWidth(const std::vector<TextRun>& runs, float size, const Font& font) const;
    void DrawRuns(const std::vector<TextRun>& runs, float x, float y, float size, const Font& font, float alpha = 1.0f) const;

    std::deque<Line> lines_;
    std::deque<Drop> drops_;
    std::vector<Splat> splats_;
    mutable std::vector<Bar> bars_;
    mutable std::vector<Label> labels_;
    mutable std::vector<Vector3> skulls_;
    float skullTimer_ = 0.0f;
    int abyssLevel_ = 0;
    std::string area_;
    float areaTime_ = 99.0f;
    int openTab_ = -1;
    int selected_ = -1;   // selected bag slot
    int useSlot_ = -1;
    CombatStyle combatStyle_ = CombatStyle::Accurate;
    const Skills* skills_ = nullptr;
    const Inventory* bag_ = nullptr;
    const Equipment* equip_ = nullptr;
    const Bank* bank_ = nullptr;
    const QuestLog* quests_ = nullptr;
    const TileMap* map_ = nullptr;
    const People* people_ = nullptr;
    Texture2D mapTex_{};
    TileCoord playerTile_{};
    std::vector<QuestMarker> markers_;
    mutable float scroll_ = 0.0f, scrollMax_ = 0.0f;   // the open page's scroll (pixels) and how far it can go
    int scrollKey_ = -99;                              // which page the scroll belongs to: a new page starts at the top
    float musicVol_ = 0.5f, sfxVol_ = 0.8f;
    DialogueView dialogue_;
    AttackKind attackKind_ = AttackKind::Melee;
    int spell_ = 0;
    bool poisoned_ = false;
    bool bankOpen_ = false;
    int bankPage_ = 0;
    int bankQty_ = 1;
    MakeMenu make_;
    bool makeOpen_ = false;
    int makePage_ = 0;
    bool helpPage_ = false;
    std::vector<std::string> playerLines_;
    int spellPage_ = 0;   // the Menu tab shows the how-to-play text instead of the settings
    int makeQty_ = 99;
    ShopMenu shop_;
    bool shopOpen_ = false, shopSell_ = false;
    int shopPage_ = 0;
    int shopQty_ = 1;

    std::vector<MenuEntry> menu_;
    Vector2 menuAt_{0, 0};
    Vector2 pointer_{-1, -1};
    std::vector<TextRun> hover_;
    int hoverMore_ = 0;
};

}  // namespace journey
