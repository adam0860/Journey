#include "game/Adventurers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "game/Gear.h"
#include "game/Progress.h"
#include "world/Pathfinder.h"

namespace journey {

namespace {

constexpr float kWalkSpeed = 3.3f;   // the hero's run speed (Player::kRunSpeed)

const char* const kRealNames[kAdvRealCount] = {"Alder", "Mirela", "Brogan", "Seren", "Tobias", "Wynne", "Kestrel", "Rowan"};
const bool kRealFemale[kAdvRealCount] = {false, true, false, true, false, true, true, false};

const char* const kFillerNames[kAdvFillerCount] = {
    "Garrick", "Isolde", "Fenwick", "Odalys", "Thorne", "Lyra", "Dunstan", "Elowen", "Corwin", "Maeve", "Hadrian", "Branwen",
    "Osric", "Tamsin", "Leofric", "Ysabel", "Godwin", "Rhosyn", "Edric", "Sunniva", "Ivo", "Petra", "Magnus", "Linnea",
    "Bertram", "Cordelia", "Alaric", "Fiora", "Gawain", "Heloise", "Jasper", "Keira", "Lucan", "Nerys", "Orsin", "Quill",
    "Iron_Mike", "FishyFran", "Dwarf_Dave", "BigBaldBob", "Sir_Loin", "AxeMaster", "TinyTim", "Cooky",
    "WoodWilly", "MineMaid", "SmithySam", "LordLogs", "Shrimp_King", "CaptainCrab", "Nooblet", "Stumpy",
    "OreOllie", "Pickle", "Mossy", "Crumbs", "Anselm", "Brigid", "Cassian", "Delphine", "Emeric", "Faye", "Gideon", "Hestia",
    "Vex", "Dread", "Morrigan", "Skarn", "Hollow", "Raven", "Grimm", "Nyx"};   // (the Abyss)

// ------------------------------------------------------------------------------------------------ what they say
const char* const kSoloGeneral[] = {
    "Anyone seen a good place to train around {area}?", "Nice weather for it.", "Is it just me or are the wolves nasty today?",
    "My feet hurt. Too much walking.", "Looking for a party to take on the {town} road.", "Does anyone know a good recipe for pie?",
    "Selling logs, cheap!", "Where can a body buy a decent sword around here?", "Level {lv} {sk}! Feels like forever.",
    "Anyone here done the quests in {town}?", "Heading to {town} for supplies.", "I should have brought more food...",
    "Lovely view from {area}.", "Be careful out there, it gets rough past the bridges.", "Any tips for {sk}?",
    "Training {sk}, slowly but surely.", "Is {town} far from here?", "Gz to whoever just levelled!", "I heard there is a dragon in the far south.",
};
const char* const kSoloChop[] = {"Another log for the pile.", "Oak is where the xp is.", "Chop, chop, chop...", "Level {lv} woodcutting now. Slow going.", "These trees never run out. Good."};
const char* const kSoloFish[] = {"The fish are biting today.", "Shrimp again...", "Anyone selling a fishing rod?", "Fishing is peaceful. I like it.", "Level {lv} fishing. Soon I'll catch the big ones."};
const char* const kSoloMine[] = {"Need coal. Always need coal.", "Clang, clang. Rock, rock.", "Anyone selling a better pickaxe?", "Level {lv} mining. Iron soon!", "Tin and copper, tin and copper."};
const char* const kSoloSmith[] = {"Hot work, this.", "Almost enough bars for a full set.", "A smith is only as good as his anvil.", "Level {lv} smithing and still no platebody."};
const char* const kSoloFight[] = {"Take that!", "Combat level {cb} and rising.", "Almost out of food...", "Ouch!", "Anyone want to trade for those bones?", "Good loot here."};
const char* const kSoloRest[] = {"Resting my feet.", "I need a bite to eat.", "Phew, that was close.", "Waiting for my hit points to come back."};

// Conversations: each line starts with A: or B: (the two speakers).
struct ConvoTemplate {
    const char* l[4];
};
const ConvoTemplate kConvos[] = {
    {{"A:Hey {you}, how is {sk} coming along?", "B:{olv} {osk} now. Slow but steady. You?", "A:Level {lv} {sk}. It gets easier.", "B:Keep at it!"}},
    {{"A:What combat level are you, {you}?", "B:{ocb}. You?", "A:{cb}. We should team up some time.", "B:Sure, but I need food first."}},
    {{"A:Anyone been out to {town}?", "B:Yes, lots to do there. Bring food.", "A:Thanks, heading there next.", "B:Good luck out there!"}},
    {{"A:Nice gear, {you}! Where did you get it?", "B:Smithed some, bought the rest.", "A:I am saving up for something like that.", "B:It takes a while, keep at it."}},
    {{"A:Do you know a quick way to {town}?", "B:Follow the road. The bridges are safe.", "A:And the wolves?", "B:Stay on the road and you will be fine."}},
    {{"A:Selling a few logs, {you}, interested?", "B:How much?", "A:Whatever the shop gives, minus nothing.", "B:Ha! Fair enough."}},
    {{"A:Did you hear about the dragon in the south?", "B:Nobody comes back from there, they say.", "A:I would not go without a lot of food.", "B:Nor would I."}},
    {{"A:I only just started. Any tips?", "B:Chop a few trees, fish a bit, then try the goblins.", "A:Goblins? Are they dangerous?", "B:Not if you bring food."}},
    {{"A:Level {lv} {sk}, can you believe it?", "B:Nice one! I am {olv} {osk}.", "A:We make a good team.", "B:Gz!"}},
    {{"A:Do you ever stop and just look at the view?", "B:Now and then. {area} is lovely.", "A:It really is.", "B:Right, back to work."}},
    {{"A:Have you tried the quests here?", "B:A few. Some of them pay really well.", "A:Which ones?", "B:Talk to the shopkeepers, they always have an errand."}},
    {{"A:Is it safe to fight round here?", "B:Depends on your level. What is yours?", "A:{cb} combat.", "B:You will be fine, mind the big ones."}},
};

std::string Replace(std::string s, const std::string& key, const std::string& value) {
    size_t at = 0;
    while ((at = s.find(key, at)) != std::string::npos) {
        s.replace(at, key.size(), value);
        at += value.size();
    }
    return s;
}

// The skill an adventurer is best at (not Hitpoints), and its level.
Skill BestSkill(const Adventurer& a, int* level) {
    Skill best = Skill::Woodcutting;
    int bl = 0;
    for (int i = 0; i < kSkillCount; ++i) {
        const Skill s = static_cast<Skill>(i);
        if (s == Skill::Hitpoints) continue;
        if (a.skills.Level(s) > bl) {
            bl = a.skills.Level(s);
            best = s;
        }
    }
    if (level) *level = bl;
    return best;
}

}  // namespace

// ------------------------------------------------------------------------------------------------ small things

double Adventurers::Rand() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (rng_ & 0xFFFFFF) / 16777216.0;
}

int Adventurers::RandInt(int n) {
    return n <= 0 ? 0 : std::min(n - 1, static_cast<int>(Rand() * n));
}

const Adventurer* Adventurers::Find(int id) const {
    for (const Adventurer& a : advs_)
        if (a.id == id) return &a;
    return nullptr;
}

std::vector<AdvChat> Adventurers::TakeChat() {
    std::vector<AdvChat> out;
    out.swap(chat_);
    return out;
}

std::string Adventurers::ItemNameLower(Item item) const {
    std::string s = GetItemInfo(item).name;
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

void Adventurers::Say(Adventurer& a, const std::string& text, float seconds, bool major) {
    a.speech.text = text;
    a.speech.left = seconds;
    AdvChat c;
    c.id = a.id;
    c.name = a.name;
    c.text = text;
    c.x = a.x;
    c.z = a.z;
    c.major = major;
    chat_.push_back(c);
    if (chat_.size() > 80) chat_.erase(chat_.begin(), chat_.begin() + 40);
}

const char* AdvFillerName(int index) { return index >= 0 && index < kAdvFillerCount ? kFillerNames[index] : "Traveller"; }

PersonLook RandomLook(unsigned* rng, bool female) {
    auto next = [rng](int n) {
        *rng ^= *rng << 13;
        *rng ^= *rng >> 17;
        *rng ^= *rng << 5;
        return static_cast<int>((*rng >> 8) % static_cast<unsigned>(n));
    };
    PersonLook l;
    l.female = female;
    l.skin = next(8);
    l.hairPick = next(12);
    l.hairColor = next(10);
    l.shirt = next(5);
    l.shirtColor = next(14);
    l.pants = next(5);
    l.pantsColor = next(14);
    l.shoes = next(5);
    l.shoesColor = next(5);
    return l;
}

void Adventurers::Walk(Adventurer& a, float dt) {
    float budget = kWalkSpeed * dt;
    while (budget > 0.0f && !a.path.empty()) {
        const float tx = a.path.front().x + 0.5f, tz = a.path.front().z + 0.5f;
        const float dx = tx - a.x, dz = tz - a.z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 1e-4f) a.facing = std::atan2(dx, dz);
        if (dist <= budget) {
            a.x = tx;
            a.z = tz;
            budget -= dist;
            a.path.pop_front();
        } else {
            a.x += dx / dist * budget;
            a.z += dz / dist * budget;
            budget = 0.0f;
        }
    }
    if (!a.path.empty()) {
        a.walkPhase += dt * kWalkSpeed * 3.2f;
        a.act = AdvAct::Walk;
    } else if (a.act == AdvAct::Walk) {
        a.act = AdvAct::Idle;
    }
}

bool Adventurers::GoTo(Adventurer& a, AdvWorld& w, TileCoord target, int radius) {
    TileMap& map = *w.map;
    if (!map.Walkable(a.Tile())) {   // (a fire lit under his feet, or a tree grown on him): step to the nearest open tile
        for (int r = 1; r <= 6; ++r) {
            bool done = false;
            for (int dz = -r; dz <= r && !done; ++dz)
                for (int dx = -r; dx <= r && !done; ++dx)
                    if (std::max(std::abs(dx), std::abs(dz)) == r && map.Walkable(a.Tile().x + dx, a.Tile().z + dz)) {
                        a.x = a.Tile().x + dx + 0.5f;
                        a.z = a.Tile().z + dz + 0.5f;
                        done = true;
                    }
            if (done) break;
        }
    }
    a.path.clear();
    if (a.Tile() == target && map.Walkable(target)) return true;
    const PathResult r = FindPathToward(map, a.Tile(), target, radius);
    if (!r.found) return false;
    a.path.assign(r.tiles.begin(), r.tiles.end());
    return true;
}

// ------------------------------------------------------------------------------------------------ the world, indexed

void Adventurers::IndexWorld(const AdvWorld& w) {
    towns_.clear();
    shopPeople_.clear();
    const TileMap& map = *w.map;
    auto centreOf = [&](const char* area, TileCoord* out) {
        bool found = false;
        for (const AreaDef& d : map.Areas())
            if (d.name == area) {
                *out = {(d.rect.x0 + d.rect.x1) / 2, (d.rect.z0 + d.rect.z1) / 2};
                found = true;
            }
        if (!found) return false;
        for (int r = 0; r <= 10; ++r)
            for (int dz = -r; dz <= r; ++dz)
                for (int dx = -r; dx <= r; ++dx)
                    if (std::max(std::abs(dx), std::abs(dz)) == r && map.Walkable(out->x + dx, out->z + dz)) {
                        *out = {out->x + dx, out->z + dz};
                        return true;
                    }
        return false;
    };
    struct T {
        const char* area;
        const char* name;
    };
    for (const T& t : {T{"Market Square", "Castle Town"}, T{"Millbrook Square", "Millbrook"}, T{"Elmswick", "Elmswick"}, T{"Saltmere", "Saltmere"},
                       T{"Lakeside", "Lakeside"}}) {
        TileCoord c;
        if (centreOf(t.area, &c)) towns_.push_back({t.name, c});
    }
    if (towns_.empty()) towns_.push_back({"the road", map.Spawn()});
    if (w.people) {
        const std::vector<Person>& all = w.people->All();
        for (size_t i = 0; i < all.size(); ++i)
            if (all[i].info->shop[0]) shopPeople_.push_back({all[i].info->shop, static_cast<int>(i)});
    }
    indexed_ = true;
}

TileCoord Adventurers::TownNear(TileCoord from) const {
    TileCoord best = towns_.empty() ? from : towns_[0].tile;
    long bd = -1;
    for (const Town& t : towns_) {
        const long d = static_cast<long>(t.tile.x - from.x) * (t.tile.x - from.x) + static_cast<long>(t.tile.z - from.z) * (t.tile.z - from.z);
        if (bd < 0 || d < bd) {
            bd = d;
            best = t.tile;
        }
    }
    return best;
}

// ------------------------------------------------------------------------------------------------ making them

void Adventurers::MakeReal(const AdvWorld& w, unsigned seed) {
    const TileMap& map = *w.map;
    unsigned look = seed * 2654435761u + 77u;
    for (int i = 0; i < kAdvRealCount; ++i) {
        Adventurer a;
        a.id = nextId_++;
        a.kind = AdvKind::Real;
        a.name = kRealNames[i];
        a.look = RandomLook(&look, kRealFemale[i]);
        // they start where the hero does: at the spawn, a few tiles apart
        TileCoord t = map.Spawn();
        for (int tries = 0; tries < 60; ++tries) {
            const int dx = static_cast<int>(Rand() * 11) - 5, dz = static_cast<int>(Rand() * 11) - 5;
            if (map.Walkable(t.x + dx, t.z + dz)) {
                t = {t.x + dx, t.z + dz};
                break;
            }
        }
        a.x = t.x + 0.5f;
        a.z = t.z + 0.5f;
        a.facing = static_cast<float>(i);
        GiveStarterKit(&a.bag);
        for (int s = 0; s < Inventory::kSlots; ++s)
            if (a.bag.Slot(s).item != Item::None) a.source[a.bag.Slot(s).item] |= kSrcStarter;
        // what he likes doing: two favourite trades, the rest in a mild order
        for (int s = 0; s < kSkillCount; ++s) a.mind.bias[s] = 0.7f + static_cast<float>(Rand()) * 0.5f;
        auto like = [&](Skill s, float k) { a.mind.bias[static_cast<int>(s)] *= k; };
        switch (i % 5) {
            case 0: like(Skill::Woodcutting, 2.2f); like(Skill::Firemaking, 1.6f); break;
            case 1: like(Skill::Fishing, 2.2f); like(Skill::Cooking, 1.6f); break;
            case 2: like(Skill::Mining, 2.2f); like(Skill::Smithing, 1.8f); break;
            case 3: like(Skill::Attack, 2.2f); like(Skill::Strength, 1.6f); like(Skill::Defence, 1.4f); like(Skill::Prayer, 1.2f); break;
            default: break;   // an all-rounder
        }
        a.mind.thinkLeft = 1.0f + i * 2.5f;   // they do not all set off in the same second
        a.chatCooldown = 8.0f + i * 5.0f;
        advs_.push_back(std::move(a));
    }
}

void Adventurers::Init(const AdvWorld& w, unsigned seed) {
    advs_.clear();
    convos_.clear();
    chat_.clear();
    nextId_ = 1;
    clock_ = 0;
    rng_ = seed * 747796405u + 2891336453u;
    if (rng_ == 0) rng_ = 1;
    shops_.Init();
    IndexWorld(w);
    MakeReal(w, seed);
    MakeFillers(w, seed);
}

// ------------------------------------------------------------------------------------------------ saving

std::string Adventurers::Save() const {
    std::ostringstream out;
    out << "adventurers=1\nclock=" << static_cast<long>(clock_) << "\n";
    for (const Adventurer& a : advs_) {
        if (a.kind != AdvKind::Real) continue;
        out << "@bot " << a.id << " " << a.name << " " << (a.look.female ? 1 : 0) << " " << a.look.skin << " " << a.look.hairPick << " " << a.look.hairColor << " "
            << a.look.shirt << " " << a.look.shirtColor << " " << a.look.pants << " " << a.look.pantsColor << " " << a.look.shoes << " " << a.look.shoesColor << " "
            << a.Tile().x << " " << a.Tile().z << "\n";
        out << "style=" << static_cast<int>(a.style) << "\n";
        if (a.pkKills > 0 || a.pkDeaths > 0) out << "pkrec=" << a.pkKills << ":" << a.pkDeaths << "\n";
        for (int s = 0; s < kSkillCount; ++s) out << "bias." << s << "=" << a.mind.bias[s] << "\n";
        for (const auto& kv : a.source) out << "src." << static_cast<int>(kv.first) << "=" << kv.second << "\n";
        out << SerializeProgress(a.skills, a.bag, a.style, &a.equip, nullptr, 0, &a.quests, nullptr, a.pvp.skull);
        out << "@end\n";
    }
    return out.str();
}

bool Adventurers::Load(const AdvWorld& w, const std::string& text, unsigned seed) {
    Init(w, seed);
    if (text.find("adventurers=1") == std::string::npos) return false;
    // read the saved real ones over the fresh ones, by name
    std::istringstream in(text);
    std::string line;
    Adventurer* cur = nullptr;
    std::string progress;
    auto flush = [&]() {
        if (!cur) return;
        ParseProgress(progress, &cur->skills, &cur->bag, &cur->style, &cur->equip, nullptr, nullptr, &cur->quests, nullptr, &cur->pvp.skull);
        cur->skills.SetHp(std::max(1, cur->skills.Hp()));
        progress.clear();
        cur = nullptr;
    };
    while (std::getline(in, line)) {
        if (line.rfind("clock=", 0) == 0) {
            clock_ = static_cast<float>(std::atof(line.c_str() + 6));
        } else if (line.rfind("@bot ", 0) == 0) {
            flush();
            std::istringstream ls(line.substr(5));
            int id, female, x, z;
            std::string name;
            PersonLook look;
            ls >> id >> name >> female >> look.skin >> look.hairPick >> look.hairColor >> look.shirt >> look.shirtColor >> look.pants >> look.pantsColor >> look.shoes >>
                look.shoesColor >> x >> z;
            for (Adventurer& a : advs_)
                if (a.kind == AdvKind::Real && a.name == name) cur = &a;
            if (!cur) continue;
            look.female = female != 0;
            cur->look = look;
            if (w.map->Walkable(x, z)) {
                cur->x = x + 0.5f;
                cur->z = z + 0.5f;
            }
            cur->source.clear();
            cur->bag = Inventory();
            cur->equip = Equipment();
        } else if (line == "@end") {
            flush();
        } else if (cur) {
            if (line.rfind("pkrec=", 0) == 0) {
                cur->pkKills = std::max(0, std::atoi(line.c_str() + 6));
                const size_t colon = line.find(':');
                if (colon != std::string::npos) cur->pkDeaths = std::max(0, std::atoi(line.c_str() + colon + 1));
            } else if (line.rfind("bias.", 0) == 0) {
                const int s = std::atoi(line.c_str() + 5);
                const size_t eq = line.find('=');
                if (s >= 0 && s < kSkillCount && eq != std::string::npos) cur->mind.bias[s] = static_cast<float>(std::atof(line.c_str() + eq + 1));
            } else if (line.rfind("src.", 0) == 0) {
                const int it = std::atoi(line.c_str() + 4);
                const size_t eq = line.find('=');
                if (it > 0 && it < static_cast<int>(Item::Count) && eq != std::string::npos)
                    cur->source[static_cast<Item>(it)] |= static_cast<unsigned>(std::atoi(line.c_str() + eq + 1));
            } else {
                progress += line + "\n";
            }
        }
    }
    flush();
    return true;
}

// ------------------------------------------------------------------------------------------------ what they say

std::string Adventurers::Activity(const Adventurer& a) const {
    if (a.kind == AdvKind::Real) return a.mind.doing;
    switch (a.filler.job) {
        case FillerJob::Chop: return "chopping trees";
        case FillerJob::Fish: return "fishing";
        case FillerJob::Mine: return "mining";
        case FillerJob::Smith: return "smithing";
        case FillerJob::Fight: return "training on the local wildlife";
        case FillerJob::Cook: return "cooking";
        case FillerJob::Wander: return "wandering about";
        case FillerJob::Hunt: return a.pvp.fightingHero ? "fighting you" : "hunting other players in the Abyss";
        default: return "taking a break";
    }
}

namespace {
std::string Fill(std::string t, const Adventurer& a, const Adventurer* b, const std::string& area, const std::string& town) {
    int lv = 0;
    const Skill best = BestSkill(a, &lv);
    t = Replace(t, "{area}", area.empty() ? "the road" : area);
    t = Replace(t, "{town}", town);
    t = Replace(t, "{cb}", std::to_string(a.Combat()));
    t = Replace(t, "{lv}", std::to_string(lv));
    t = Replace(t, "{sk}", SkillName(best));
    if (b) {
        int olv = 0;
        const Skill obest = BestSkill(*b, &olv);
        t = Replace(t, "{you}", b->name);
        t = Replace(t, "{ocb}", std::to_string(b->Combat()));
        t = Replace(t, "{olv}", std::to_string(olv));
        t = Replace(t, "{osk}", SkillName(obest));
    }
    return t;
}
}  // namespace

std::string Adventurers::Line(const Adventurer& a, const std::function<double()>& roll) const {
    auto pick = [&](const char* const* v, size_t n) { return std::string(v[std::min(n - 1, static_cast<size_t>(roll() * n))]); };
    std::string t;
    const double r = roll();
    switch (a.act) {
        case AdvAct::Chop: t = r < 0.5 ? pick(kSoloChop, sizeof(kSoloChop) / sizeof(*kSoloChop)) : std::string(); break;
        case AdvAct::Fish: t = r < 0.5 ? pick(kSoloFish, sizeof(kSoloFish) / sizeof(*kSoloFish)) : std::string(); break;
        case AdvAct::Mine: t = r < 0.5 ? pick(kSoloMine, sizeof(kSoloMine) / sizeof(*kSoloMine)) : std::string(); break;
        case AdvAct::Smelt:
        case AdvAct::Smith: t = r < 0.5 ? pick(kSoloSmith, sizeof(kSoloSmith) / sizeof(*kSoloSmith)) : std::string(); break;
        case AdvAct::Fight: t = r < 0.5 ? pick(kSoloFight, sizeof(kSoloFight) / sizeof(*kSoloFight)) : std::string(); break;
        case AdvAct::Rest: t = r < 0.6 ? pick(kSoloRest, sizeof(kSoloRest) / sizeof(*kSoloRest)) : std::string(); break;
        default: break;
    }
    if (t.empty()) t = pick(kSoloGeneral, sizeof(kSoloGeneral) / sizeof(*kSoloGeneral));
    std::string area;
    if (map_) area = map_->AreaAt(a.Tile().x, a.Tile().z);
    const TileCoord home = TownNear(a.Tile());
    std::string town = "town";
    for (const Town& tw : towns_)
        if (tw.tile == home) town = tw.name;
    return Fill(t, a, nullptr, area, town);
}

void Adventurers::Converse(Adventurer& a, Adventurer& b, AdvWorld& w) {
    const ConvoTemplate& tpl = kConvos[RandInt(static_cast<int>(sizeof(kConvos) / sizeof(*kConvos)))];
    std::string area = w.map->AreaAt(a.Tile().x, a.Tile().z);
    const TileCoord home = TownNear(a.Tile());
    std::string town = "town";
    for (const Town& tw : towns_)
        if (tw.tile == home) town = tw.name;
    Convo c;
    c.a = a.id;
    c.b = b.id;
    const int count = 3 + RandInt(2);
    for (int i = 0; i < count && tpl.l[i]; ++i) {
        const std::string raw = tpl.l[i];
        const bool aSpeaks = raw[0] == 'A';
        Adventurer& speaker = aSpeaks ? a : b;
        Adventurer& other = aSpeaks ? b : a;
        c.lines.push_back({speaker.id, Fill(raw.substr(2), speaker, &other, area, town)});
    }
    c.timer = 0.4f;
    a.chatPartner = b.id;
    b.chatPartner = a.id;
    convos_.push_back(std::move(c));
}

void Adventurers::UpdateChat(Adventurer& a, float dt, AdvWorld& w) {
    a.chatCooldown -= dt;
    if (a.chatPartner >= 0 || a.chatCooldown > 0.0f) return;
    const bool still = a.path.empty() && a.act != AdvAct::Walk && a.act != AdvAct::Fight;
    const float hx = static_cast<float>(w.hero.x) - a.x, hz = static_cast<float>(w.hero.z) - a.z;
    if (!still || hx * hx + hz * hz > 30.0f * 30.0f) {   // (nobody is listening out there: save the breath)
        a.chatCooldown = 4.0f + static_cast<float>(Rand()) * 4.0f;
        return;
    }
    Adventurer* partner = nullptr;
    float bd = 4.6f * 4.6f;
    for (Adventurer& b : advs_) {
        if (&b == &a || b.chatPartner >= 0 || b.chatCooldown > 0.0f || !b.path.empty() || b.act == AdvAct::Walk || b.act == AdvAct::Fight) continue;
        const float dx = b.x - a.x, dz = b.z - a.z;
        if (dx * dx + dz * dz < bd) {
            bd = dx * dx + dz * dz;
            partner = &b;
        }
    }
    if (partner) {
        Converse(a, *partner, w);
    } else if (Rand() < 0.55) {
        Say(a, Line(a, [this]() { return Rand(); }));
    }
    a.chatCooldown = 22.0f + static_cast<float>(Rand()) * 40.0f;
}

// ------------------------------------------------------------------------------------------------ examine, talk, hiscores

std::string Adventurers::ExamineText(int id) const {
    const Adventurer* a = Find(id);
    if (!a) return "";
    return a->name + ": a fellow adventurer. Combat level " + std::to_string(a->Combat()) + ", total level " + std::to_string(a->skills.TotalLevel()) + ". " +
           (a->kind == AdvKind::Real ? "Looks like " : "Looks like ") + "they are " + Activity(*a) + ".";
}

std::string Adventurers::TalkLine(int id, const std::function<double()>& roll) {
    Adventurer* a = nullptr;
    for (Adventurer& x : advs_)
        if (x.id == id) a = &x;
    if (!a) return "";
    static const char* const tips[] = {"Keep your food topped up.", "The shops give a fair price for ore and fish.", "Quests pay better than they look.",
                                       "Mind the big ones, they hit hard.", "Bury the bones, the prayers add up.", "A better axe makes all the difference."};
    int lv = 0;
    const Skill best = BestSkill(*a, &lv);
    std::string s = "I'm " + Activity(*a) + ". Level " + std::to_string(lv) + " " + SkillName(best) + " so far, combat " + std::to_string(a->Combat()) + ". " +
                    tips[std::min<size_t>(5, static_cast<size_t>(roll() * 6))];
    Say(*a, s, 6.0f);
    return s;
}

std::vector<AdvRank> Adventurers::Ranking(const std::string& heroName, int heroTotal, int heroCombat) const {
    std::vector<AdvRank> r;
    for (const Adventurer& a : advs_) r.push_back({a.name, a.skills.TotalLevel(), a.Combat(), false, a.kind == AdvKind::Real});
    r.push_back({heroName, heroTotal, heroCombat, true, false});
    std::stable_sort(r.begin(), r.end(), [](const AdvRank& x, const AdvRank& y) { return x.total > y.total; });
    return r;
}

bool Adventurers::Honest(std::string* why) const {
    for (const Adventurer& a : advs_) {
        if (a.kind != AdvKind::Real) continue;
        for (int s = 0; s < kEquipSlotCount; ++s) {
            const Item it = a.equip.Get(static_cast<EquipSlot>(s));
            if (it == Item::None) continue;
            auto f = a.source.find(it);
            if (f == a.source.end() || f->second == 0 || f->second == kSrcStarter) {
                if (why) *why = a.name + " wears " + GetItemInfo(it).name + " that came from nowhere";
                return false;
            }
        }
        for (int i = 0; i < Inventory::kSlots; ++i) {
            const Item it = a.bag.Slot(i).item;
            if (it == Item::None) continue;
            auto f = a.source.find(it);
            if (f == a.source.end() || f->second == 0) {
                if (why) *why = a.name + " carries " + GetItemInfo(it).name + " that came from nowhere";
                return false;
            }
        }
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ the main loop

void Adventurers::Update(float dt, AdvWorld& w) {
    if (!indexed_) IndexWorld(w);
    map_ = w.map;
    clock_ += dt;
    world_ = &w;
    shops_.Update(dt);
    UpdateBeasts(dt, w);
    for (Adventurer& a : advs_) {
        a.pvp.skull = std::max(0.0f, a.pvp.skull - dt);
        if (a.pvp.dead) {   // away for a while: then back, with the same gear as ever
            a.pvp.respawn -= dt;
            if (a.pvp.respawn <= 0.0f) PvpRespawn(a, w);
            continue;
        }
        if (a.kind == AdvKind::Filler && a.skills.Hp() < a.skills.MaxHp()) {   // the static ones mend like everybody (a hit point a minute)
            a.mind.regen += dt;
            if (a.mind.regen >= kRegenSeconds) {
                a.mind.regen -= kRegenSeconds;
                a.skills.Heal(1);
            }
        }
        a.actTime += dt;
        a.strikeAge += dt;
        a.hurtAge += dt;
        a.speech.left -= dt;
        if (a.speech.left <= 0.0f) a.speech.text.clear();
        Walk(a, dt);
        if (a.kind == AdvKind::Real) UpdateReal(a, dt, w);
        else UpdateFiller(a, dt, w);
        UpdateChat(a, dt, w);
    }
    // conversations: one line every few seconds, the two of them facing each other if they are just standing there
    for (size_t i = 0; i < convos_.size();) {
        Convo& c = convos_[i];
        Adventurer* x = nullptr;
        Adventurer* y = nullptr;
        for (Adventurer& a : advs_) {
            if (a.id == c.a) x = &a;
            if (a.id == c.b) y = &a;
        }
        bool done = !x || !y;
        if (!done) {
            const float dx = y->x - x->x, dz = y->z - x->z;
            if (dx * dx + dz * dz > 9.0f * 9.0f) done = true;   // one of them walked off
            for (Adventurer* p : {x, y})
                if (p->path.empty() && (p->act == AdvAct::Idle || p->act == AdvAct::Chat || p->act == AdvAct::Rest)) {
                    Adventurer* q = p == x ? y : x;
                    p->facing = std::atan2(q->x - p->x, q->z - p->z);
                    if (p->kind == AdvKind::Filler) p->act = AdvAct::Chat;
                }
        }
        if (!done) {
            c.timer -= dt;
            if (c.timer <= 0.0f) {
                if (c.next >= c.lines.size()) {
                    done = true;
                } else {
                    Adventurer* speaker = c.lines[c.next].first == x->id ? x : y;
                    Say(*speaker, c.lines[c.next].second, 3.4f);
                    ++c.next;
                    c.timer = 3.6f;
                }
            }
        }
        if (done) {
            if (x) {
                x->chatPartner = -1;
                x->chatCooldown = 40.0f + static_cast<float>(Rand()) * 80.0f;
            }
            if (y) {
                y->chatPartner = -1;
                y->chatCooldown = 40.0f + static_cast<float>(Rand()) * 80.0f;
            }
            convos_.erase(convos_.begin() + static_cast<long>(i));
        } else {
            ++i;
        }
    }
}

void Adventurers::FastForward(float seconds, AdvWorld& w, float step) {
    for (float t = 0; t < seconds; t += step) {
        if (w.forest) w.forest->Update(step);
        if (w.rocks) w.rocks->Update(step);
        if (w.fires && w.map) w.fires->Update(step, w.map);
        if (w.npcs && w.map) w.npcs->Update(step, *w.map, {-100, -100}, w.roll);
        Update(step, w);
    }
}

}  // namespace journey
