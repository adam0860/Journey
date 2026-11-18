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

#include "game/Combat.h"
#include "game/Equipment.h"
#include "game/Inventory.h"
#include "game/Menu.h"
#include "game/Skills.h"
#include "raylib.h"

namespace journey {

class Hud {
public:
    static constexpr int kTabBag = 0, kTabSkills = 1, kTabGear = 2, kTabCombat = 5, kTabMenu = 6;

    // The HUD reads (never changes) the player's skills and bag. Call once at start-up.
    void Bind(const Skills* skills, const Inventory* bag, const Equipment* equip = nullptr) {
        skills_ = skills;
        bag_ = bag;
        equip_ = equip;
    }
    void SetCombatStyle(CombatStyle s) { combatStyle_ = s; }

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
    // Which combat style button (0..3) is under this point, or -1 (also -1 while the Combat tab is closed).
    int StyleAt(Vector2 p) const;

    // Which worn-gear slot (EquipSlot as an int) is under this point, or -1 (also -1 while the Gear tab is closed).
    int EquipSlotAt(Vector2 p) const;

    // ---- the make panel (smelting at a furnace, smithing at an anvil) ----
    enum class MakeHitKind { None, Option, Tier, Qty, Close };
    struct MakeHit {
        MakeHitKind kind = MakeHitKind::None;
        int index = 0;   // option index / tier / quantity (1, 5 or 99 = all)
    };
    void OpenMake(MakeMenu menu) {
        make_ = std::move(menu);
        makeOpen_ = true;
    }
    void CloseMake() { makeOpen_ = false; }
    bool MakeOpen() const { return makeOpen_; }
    const MakeMenu& Make() const { return make_; }
    int MakeQuantity() const { return makeQty_; }
    void SetMakeQuantity(int q) { makeQty_ = q; }
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
    Rectangle MakeTierRect(int t) const;
    Rectangle MakeQtyRect(int i) const;
    Rectangle MakeCloseRect() const;
    Rectangle ShopRectOf() const;
    Rectangle ShopRowRect(int i) const;
    Rectangle ShopTabRect(int t) const;
    Rectangle ShopQtyRect(int i) const;
    Rectangle ShopPageRect(int i) const;
    Rectangle ShopCloseRect() const;
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
    std::string area_;
    float areaTime_ = 99.0f;
    int openTab_ = -1;
    int selected_ = -1;   // selected bag slot
    int useSlot_ = -1;
    CombatStyle combatStyle_ = CombatStyle::Accurate;
    const Skills* skills_ = nullptr;
    const Inventory* bag_ = nullptr;
    const Equipment* equip_ = nullptr;
    MakeMenu make_;
    bool makeOpen_ = false;
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
