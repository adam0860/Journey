#include "game/Quests.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include "game/Gear.h"

namespace journey {

namespace {

QuestStep Kill(const char* what, Species s, int n, const char* sayNothing = "", int kind = kAnyWeapon) {
    QuestStep st;
    st.kind = StepKind::Kill;
    st.objective = what;
    st.species = s;
    st.count = n;
    st.killKind = kind;
    st.say = sayNothing;
    return st;
}

QuestStep Have(const char* what, const char* person, Item item, int n, const char* say, bool take = true) {
    QuestStep st;
    st.kind = StepKind::Have;
    st.objective = what;
    st.person = person;
    st.item = item;
    st.count = n;
    st.take = take;
    st.say = say;
    return st;
}

QuestStep Reach(const char* what, const char* area) {
    QuestStep st;
    st.kind = StepKind::Reach;
    st.objective = what;
    st.area = area;
    return st;
}

QuestStep Talk(const char* what, const char* person, const char* say) {
    QuestStep st;
    st.kind = StepKind::Talk;
    st.objective = what;
    st.person = person;
    st.say = say;
    return st;
}

QuestStep Use(const char* what, Item item, const char* area, bool take, Item give, const char* say, Item tool = Item::None) {
    QuestStep st;
    st.kind = StepKind::Use;
    st.objective = what;
    st.item = item;
    st.area = area;
    st.take = take;
    st.give = give;
    st.say = say;
    st.tool = tool;
    return st;
}

Item G(Tier t, GearType g) { return GearItem(t, g); }

std::vector<QuestInfo> Build() {
    std::vector<QuestInfo> v;
    auto add = [&](QuestInfo q) { v.push_back(std::move(q)); };

    {   // ---- the tutorial
        QuestInfo q;
        q.key = "welcome";
        q.name = "Welcome to Journey";
        q.giver = "guard_gate";
        q.intro = "New in town? Every traveller starts the same way: with an axe and an empty stomach. Bring me three logs, then a cooked shrimp, and I'll make it worth your while.";
        q.steps = {Have("Bring 3 logs to the castle guard", "guard_gate", Item::Logs, 3, "Good, solid logs. I'll keep these for the watch fire - get yourself another few for cooking.", true),
                   Have("Bring a cooked shrimp to the castle guard", "guard_gate", Item::Shrimps, 1,
                        "Perfectly done! You can fend for yourself. The fishing spots are along the river, and a fire needs logs and a tinderbox.")};
        q.outro = "Off you go, adventurer. The road south leads to Millbrook, but mind the goblins.";
        q.reward.coins = 60;
        q.reward.xp = {{Skill::Cooking, 60}, {Skill::Firemaking, 60}, {Skill::Woodcutting, 40}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "chicken_run";
        q.name = "Chicken Run";
        q.giver = "aubrey";
        q.intro = "My shelves are bare of poultry. Fancy a little hunting? Kill three chickens in the coop and bring me their meat.";
        q.steps = {Kill("Kill 3 chickens", Species::Chicken, 3),
                   Have("Bring 3 raw chicken to Aubrey", "aubrey", Item::RawChicken, 3, "Lovely and fresh. Here is your pay.")};
        q.outro = "Come back any time you want to sell more.";
        q.reward.coins = 60;
        q.reward.xp = {{Skill::Attack, 120}, {Skill::Hitpoints, 40}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "hildas_order";
        q.name = "Hilda's Order";
        q.giver = "hilda";
        q.intro = "I have a big pie order and nothing to put in it! Bring me three eggs and two cooked chickens, and I'll pay you well.";
        q.steps = {Have("Bring 3 eggs to Hilda", "hilda", Item::Egg, 3, "Fresh as can be.", true),
                   Have("Bring 2 cooked chickens to Hilda", "hilda", Item::Chicken, 2, "Beautifully browned. You have a cook's touch!")};
        q.outro = "The whole village will eat well tonight.";
        q.reward.coins = 90;
        q.reward.xp = {{Skill::Cooking, 250}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "lumberjacks_lament";
        q.name = "The Lumberjack's Lament";
        q.giver = "old_tom";
        q.intro = "My old back can't swing an axe any more, and winter is coming. Chop me fifteen logs and I'll pay you a fair price.";
        q.steps = {Have("Bring 15 logs to Old Tom", "old_tom", Item::Logs, 15, "That will keep the farmhouse warm until spring.")};
        q.outro = "Bless you. A strong back is a gift.";
        q.reward.coins = 130;
        q.reward.xp = {{Skill::Woodcutting, 300}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "fishermans_friend";
        q.name = "The Fisherman's Friend";
        q.giver = "fisher";
        q.intro = "The fish are biting but my hands are slow these days. Net me twelve shrimps and I will share my best secret.";
        q.steps = {Have("Bring 12 shrimps to Finn", "fisher", Item::Shrimps, 12, "Plump ones! You have the patience for it.")};
        q.outro = "The secret: stand where the water swirls. It is not much of a secret, but it works.";
        q.reward.coins = 150;
        q.reward.xp = {{Skill::Fishing, 400}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "goblin_trouble";
        q.name = "Goblin Trouble";
        q.giver = "guard_village";
        q.intro = "Those goblins up the road are getting bold. They raid the farms and scare the traders. If you are handy with a weapon, kill six of them and report back.";
        q.minCombat = 3;
        q.steps = {Kill("Kill 6 goblins in the camp on the road", Species::Goblin, 6),
                   Talk("Report back to the village guard", "guard_village", "Six fewer goblins. The traders will sleep better. Here is your reward.")};
        q.outro = "There is more trouble where that came from, if you want it.";
        q.reward.coins = 150;
        q.reward.xp = {{Skill::Attack, 250}, {Skill::Strength, 250}, {Skill::Hitpoints, 150}};
        q.reward.items = {{G(Tier::Iron, GearType::Helm), 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "goblin_chief";
        q.name = "The Goblin Warriors";
        q.giver = "guard_village";
        q.intro = "The little ones were only the start. There are warriors in that camp, in iron caps with horns. Kill three of them and I will be in your debt.";
        q.after = "goblin_trouble";
        q.minCombat = 8;
        q.steps = {Kill("Kill 3 goblin warriors", Species::GoblinWarrior, 3),
                   Talk("Report back to the village guard", "guard_village", "The camp is leaderless. You are a credit to Millbrook.")};
        q.outro = "Take this. It has served me well.";
        q.reward.coins = 300;
        q.reward.xp = {{Skill::Attack, 400}, {Skill::Strength, 400}, {Skill::Defence, 400}};
        q.reward.items = {{G(Tier::Iron, GearType::Body), 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "ore_for_the_smith";
        q.name = "Ore for the Smith";
        q.giver = "wilf";
        q.intro = "I have a rush order for bronze and the quarry boys are late. Bring me four copper ore and four tin ore and I will pay you in coin and know-how.";
        q.steps = {Have("Bring 4 copper ore to Wilf", "wilf", Item::CopperOre, 4, "Good copper.", true),
                   Have("Bring 4 tin ore to Wilf", "wilf", Item::TinOre, 4, "And good tin. Right, back to the anvil!")};
        q.outro = "Come by whenever you want to learn smithing properly. Torvald in the castle will sell you a mithril pickaxe now.";
        q.reward.coins = 120;
        q.reward.xp = {{Skill::Mining, 150}, {Skill::Smithing, 200}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "a_sharp_order";
        q.name = "A Sharp Order";
        q.giver = "torvald";
        q.intro = "A knight wants a bronze sword by the morning and I'm swamped. Smith one yourself and bring it here - I will pay you well for it.";
        q.reqSkill = Skill::Smithing;
        q.reqLevel = 4;
        q.steps = {Have("Bring a bronze sword to Torvald", "torvald", GearItem(Tier::Bronze, GearType::Sword), 1,
                        "Hmm. A clean edge and a true balance. You have the knack.")};
        q.outro = "Here. A proper pickaxe, for a proper smith. My shelf has steel ones for you now.";
        q.reward.coins = 100;
        q.reward.xp = {{Skill::Smithing, 350}};
        q.reward.items = {{Item::IronPickaxe, 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "bowyers_trial";
        q.name = "The Bowyer's Trial";
        q.giver = "larson";
        q.intro = "Anyone can swing a sword. A real archer drops his target before it reaches him. Shoot four goblins with a bow and I'll know you are the real thing.";
        q.minCombat = 3;
        q.steps = {Kill("Kill 4 goblins with a bow", Species::Goblin, 4, "", static_cast<int>(AttackKind::Ranged)),
                   Talk("Tell Larson", "larson", "Four goblins and not a scratch! Here is a bow worthy of you.")};
        q.outro = "Mind your arrows. They break more often than they should. I keep yew bows for people like you now.";
        q.reward.coins = 100;
        q.reward.xp = {{Skill::Ranged, 500}};
        q.reward.items = {{Item::OakShortbow, 1}, {Item::IronArrows, 60}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "wizards_test";
        q.name = "The Wizard's Test";
        q.giver = "aldric";
        q.intro = "Magic is not for the faint of heart, or the short of runes. Cast spells at four goblins from your staff and come back alive. Take a handful of runes to start you off.";
        q.minCombat = 3;
        q.steps = {Kill("Kill 4 goblins with magic", Species::Goblin, 4, "", static_cast<int>(AttackKind::Magic)),
                   Talk("Tell Aldric", "aldric", "Splendid! The flames suit you. A staff of air, for your trouble.")};
        q.outro = "Study hard. The Abyss is full of mages, and they are not so gentle. My shop sells the elemental staffs to you now.";
        q.reward.coins = 100;
        q.reward.xp = {{Skill::Magic, 500}};
        q.reward.items = {{Item::AirStaff, 1}, {Item::FireRune, 40}, {Item::AirRune, 60}, {Item::MindRune, 60}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "wolf_hunt";
        q.name = "The Wolf Hunt";
        q.giver = "old_tom";
        q.intro = "Wolves have been at my sheep, down in the wood past the lake. Three should do. They run in packs, so don't go in unprepared.";
        q.minCombat = 10;
        q.steps = {Kill("Kill 3 wolves in the wood by the lake", Species::Wolf, 3),
                   Talk("Tell Old Tom", "old_tom", "Three wolves! The flock thanks you. Here, take what I can spare.")};
        q.outro = "You have a hunter's heart, lad.";
        q.reward.coins = 250;
        q.reward.xp = {{Skill::Strength, 400}, {Skill::Hitpoints, 300}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "spiders_in_the_dark";
        q.name = "Spiders in the Dark";
        q.giver = "bram";
        q.intro = "Something has been nesting in the lower tunnels - giant spiders, venomous as anything. Kill four, and bring me five coal for the forge while you are down there.";
        q.minCombat = 15;
        q.steps = {Kill("Kill 4 giant spiders in the Mining Caves", Species::GiantSpider, 4),
                   Have("Bring 5 coal to Bram", "bram", Item::Coal, 5, "That poison nearly had me last time. You have saved the mine.")};
        q.outro = "The caves are yours. There is mithril, deeper in.";
        q.reward.coins = 300;
        q.reward.xp = {{Skill::Mining, 600}, {Skill::Defence, 200}};
        q.reward.items = {{Item::MithrilOre, 2}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "bones_of_the_crypt";
        q.name = "Bones of the Crypt";
        q.giver = "priest";
        q.intro = "The dead do not rest in the crypt beneath the castle. Go down, put six skeletons back to sleep, and bring their bones for a proper burial.";
        q.minCombat = 20;
        q.steps = {Reach("Enter the Castle Crypt", "Castle Crypt"),
                   Kill("Lay six skeletons to rest", Species::Skeleton, 6),
                   Have("Bring 6 bones to Father Eamon", "priest", Item::Bones, 6, "May they find peace. Bless you, child.")};
        q.outro = "Bury what you carry, and the prayers will come easily.";
        q.reward.coins = 300;
        q.reward.xp = {{Skill::Prayer, 900}, {Skill::Defence, 200}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "the_dragon";
        q.name = "The Dragon of the Desert";
        q.giver = "king";
        q.intro = "A dragon has made its lair in the ruins of the desert, and my scouts do not return. You have proven yourself many times over. Slay the beast and bring me proof: its bones.";
        q.minCombat = 40;
        q.minPoints = 6;
        q.steps = {Reach("Travel to the Desert Ruins", "Desert Ruins"),
                   Kill("Slay the dragon", Species::Dragon, 1),
                   Have("Bring the dragon's bones to the King", "king", Item::DragonBones, 1, "By the crown... it is true. The realm is in your debt.")};
        q.outro = "Take this from the royal vault. You have earned a hero's title.";
        q.reward.coins = 3000;
        q.reward.xp = {{Skill::Attack, 2500}, {Skill::Strength, 2500}, {Skill::Defence, 2500}, {Skill::Hitpoints, 2500}};
        q.reward.items = {{G(Tier::Mithril, GearType::Sword), 1}, {G(Tier::Mithril, GearType::Shield), 1}};
        add(q);
    }
    // ---- crafting: Ellis's three lessons
    {
        QuestInfo q;
        q.key = "leather_lessons";
        q.name = "Leather Lessons";
        q.giver = "ellis";
        q.intro = "Leather is dear and good tanners are rare. Tan a cowhide at the rack, then sew yourself a leather cap with a needle and thread. Bring it to me and I'll show you the finer points.";
        q.steps = {Have("Bring a leather cap to Ellis", "ellis", Item::LeatherCap, 1, "Neat stitching! You have patient hands.")};
        q.outro = "Here is some thread for your next project.";
        q.reward.coins = 90;
        q.reward.xp = {{Skill::Crafting, 250}};
        q.reward.items = {{Item::Thread, 10}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "potters_plea";
        q.name = "The Potter's Plea";
        q.giver = "ellis";
        q.intro = "The inn needs pots, and I have no time to throw them. Dig some clay, spin three pots on the wheel and bring them over.";
        q.after = "leather_lessons";
        q.steps = {Have("Bring 3 pots to Ellis", "ellis", Item::Pot, 3, "Round and even. The innkeeper will be pleased.")};
        q.outro = "Come back if you fancy something shinier.";
        q.reward.coins = 100;
        q.reward.xp = {{Skill::Crafting, 200}};
        q.reward.items = {{Item::Clay, 5}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "ring_of_fortune";
        q.name = "A Ring of Fortune";
        q.giver = "ellis";
        q.intro = "A traveller wants a gold ring for his bride. Smelt a gold bar, cast it in a ring mould at the jeweller's bench and bring me the result.";
        q.after = "potters_plea";
        q.reqSkill = Skill::Crafting;
        q.reqLevel = 5;
        q.steps = {Have("Bring a gold ring to Ellis", "ellis", Item::GoldRing, 1, "It gleams! She will say yes for certain.")};
        q.outro = "A fine day's work. This sapphire was in the setting - keep it.";
        q.reward.coins = 200;
        q.reward.xp = {{Skill::Crafting, 400}};
        q.reward.items = {{Item::UncutSapphire, 1}};
        add(q);
    }
    // ---- fishing at Lakeside
    {
        QuestInfo q;
        q.key = "anglers_challenge";
        q.name = "The Angler's Challenge";
        q.giver = "gerrant";
        q.intro = "Anyone can net a shrimp. Show me you can fly-fish: five trout, caught with a rod and feathers, and I'll make it worth your while.";
        q.reqSkill = Skill::Fishing;
        q.reqLevel = 20;
        q.steps = {Have("Bring 5 trout to Gerrant", "gerrant", Item::Trout, 5, "Plump and spotted. You have the touch.")};
        q.outro = "Have some feathers. You will need plenty.";
        q.reward.coins = 250;
        q.reward.xp = {{Skill::Fishing, 600}};
        q.reward.items = {{Item::Feathers, 100}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "the_big_one";
        q.name = "The Big One";
        q.giver = "silas";
        q.intro = "There are tuna in the deep water that would make a grown man weep. Harpoon three of them, cook them, and I'll tell you where the best fishing is.";
        q.after = "anglers_challenge";
        q.reqSkill = Skill::Fishing;
        q.reqLevel = 35;
        q.steps = {Have("Bring 3 tuna to Silas", "silas", Item::Tuna, 3, "Magnificent! I haven't seen fish like that in years.")};
        q.outro = "The secret is patience. And a very sharp harpoon.";
        q.reward.coins = 400;
        q.reward.xp = {{Skill::Fishing, 1000}, {Skill::Cooking, 300}};
        add(q);
    }
    // ---- mining and the mountains
    {
        QuestInfo q;
        q.key = "mithril_dreams";
        q.name = "Mithril Dreams";
        q.giver = "dunmore";
        q.intro = "Every trader dreams of mithril. Dig me three lumps of the blue ore from the deep caves and I'll pay you better than any shop would.";
        q.reqSkill = Skill::Mining;
        q.reqLevel = 30;
        q.steps = {Have("Bring 3 mithril ore to Dunmore", "dunmore", Item::MithrilOre, 3, "Look at that blue! Honest work, friend.")};
        q.outro = "Here is steel for the road, with my thanks.";
        q.reward.coins = 400;
        q.reward.xp = {{Skill::Mining, 700}};
        q.reward.items = {{Item::SteelBar, 5}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "trolls_of_the_pass";
        q.name = "Trolls of the Pass";
        q.giver = "bram";
        q.intro = "The mountain trolls block the road to the caves. They are big, slow and hit like a falling tree. Kill two of them and the miners can come back.";
        q.after = "spiders_in_the_dark";
        q.minCombat = 30;
        q.steps = {Kill("Kill 2 mountain trolls in the Troll Mountains", Species::MountainTroll, 2),
                   Talk("Tell Bram", "bram", "Two trolls down! The road is open again.")};
        q.outro = "Steel plate for the one who cleared the pass.";
        q.reward.coins = 500;
        q.reward.xp = {{Skill::Strength, 900}, {Skill::Defence, 600}, {Skill::Hitpoints, 400}};
        q.reward.items = {{G(Tier::Steel, GearType::Legs), 1}};
        add(q);
    }
    // ---- the desert and the Abyss
    {
        QuestInfo q;
        q.key = "curse_of_the_mummies";
        q.name = "The Curse of the Mummies";
        q.giver = "priest";
        q.intro = "The dead of the desert ruins walk again, wrapped in bandages and bound to the sand. Lay three of them to rest, child. Take care: they strike from a distance.";
        q.after = "bones_of_the_crypt";
        q.minCombat = 45;
        q.steps = {Kill("Lay 3 mummies to rest in the Desert Ruins", Species::DesertMummy, 3),
                   Talk("Tell Father Eamon", "priest", "Three more souls at peace. Heaven bless your sword arm.")};
        q.outro = "Your prayers will be stronger for it.";
        q.reward.coins = 500;
        q.reward.xp = {{Skill::Prayer, 1500}, {Skill::Defence, 500}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "into_the_abyss";
        q.name = "Into the Abyss";
        q.giver = "king";
        q.intro = "South of the desert lies the Abyss, where warriors, archers and mages roam like wolves. Their champions lead them. Defeat two and bring the realm peace. Beware: whatever you carry there, you may lose.";
        q.after = "the_dragon";
        q.minCombat = 60;
        q.steps = {Reach("Enter the Abyss", "The Abyss"),
                   Kill("Defeat 2 Abyss champions", Species::AbyssChampion, 2),
                   Talk("Report to the King", "king", "The Abyss champions fallen! You are a legend, traveller.")};
        q.outro = "Take this plate. It was my father's.";
        q.reward.coins = 5000;
        q.reward.xp = {{Skill::Attack, 3500}, {Skill::Strength, 3500}, {Skill::Defence, 3500}, {Skill::Hitpoints, 3500}};
        q.reward.items = {{G(Tier::Mithril, GearType::Body), 1}};
        add(q);
    }
    // ---- trouble with the new creatures
    {
        QuestInfo q;
        q.key = "road_robbers";
        q.name = "Road Robbers";
        q.giver = "guard_village";
        q.intro = "Bandits have been robbing the travellers on the road. Take care of three of them and the merchants will sleep easier.";
        q.minCombat = 14;
        q.steps = {Kill("Defeat 3 bandits on the road", Species::Bandit, 3),
                   Talk("Tell the village guard", "guard_village", "Three fewer thieves. The road thanks you.")};
        q.outro = "Here, a little something from the village purse.";
        q.reward.coins = 400;
        q.reward.xp = {{Skill::Attack, 700}, {Skill::Defence, 500}, {Skill::Hitpoints, 300}};
        q.reward.items = {{G(Tier::Iron, GearType::Legs), 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "dark_magic";
        q.name = "Dark Magic";
        q.giver = "aldric";
        q.intro = "A pupil of mine turned to dark magic and now his friends haunt the old crypt. Stop two of them, if you have the nerve and a spell or two.";
        q.reqSkill = Skill::Magic;
        q.reqLevel = 8;
        q.steps = {Kill("Defeat 2 dark wizards", Species::DarkWizard, 2),
                   Talk("Tell Aldric", "aldric", "Both gone! You have a gift. Take these, you will need them.")};
        q.outro = "Magic is a craft. Keep practising. I will sell you chaos runes from now on.";
        q.reward.coins = 300;
        q.reward.xp = {{Skill::Magic, 1500}, {Skill::Hitpoints, 300}};
        q.reward.items = {{Item::WaterStaff, 1}, {Item::ChaosRune, 30}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "imp_infestation";
        q.name = "Imp Infestation";
        q.giver = "ellis";
        q.intro = "Little red imps keep stealing my thread and my moulds. They hide in the crypt. Catch four of them for me!";
        q.minCombat = 5;
        q.steps = {Kill("Defeat 4 imps in the crypt", Species::Imp, 4),
                   Talk("Tell Ellis", "ellis", "My thread is safe! Take a little gem for your trouble.")};
        q.outro = "Cut it yourself with a chisel. It is good practice.";
        q.reward.coins = 200;
        q.reward.xp = {{Skill::Crafting, 900}, {Skill::Attack, 300}};
        q.reward.items = {{Item::UncutSapphire, 2}, {Item::UncutEmerald, 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "ogre_trouble";
        q.name = "Ogre Trouble";
        q.giver = "bram";
        q.intro = "With the trolls gone, the ogres moved into the foothills. They are slower than trolls but twice as angry. Kill two, and I will make it worth your while.";
        q.after = "trolls_of_the_pass";
        q.minCombat = 38;
        q.steps = {Kill("Defeat 2 ogres in the Troll Mountains", Species::Ogre, 2),
                   Talk("Tell Bram", "bram", "Two ogres! The foothills are safe. This was my grandfather's lucky charm.")};
        q.outro = "It smells of troll, but it works.";
        q.reward.coins = 900;
        q.reward.xp = {{Skill::Strength, 1400}, {Skill::Defence, 900}, {Skill::Hitpoints, 600}};
        q.reward.items = {{Item::TrollToothAmulet, 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "scorpion_stings";
        q.name = "Scorpion Stings";
        q.giver = "priest";
        q.intro = "Pilgrims to the desert ruins come back swollen and feverish. Giant scorpions are to blame. Kill four of them, and be careful of the poison.";
        q.after = "curse_of_the_mummies";
        q.minCombat = 30;
        q.steps = {Kill("Kill 4 scorpions in the Desert Ruins", Species::Scorpion, 4),
                   Talk("Tell Father Eamon", "priest", "The pilgrims can pass in peace again. Bless you.")};
        q.outro = "A pilgrim left this behind. It should help someone.";
        q.reward.coins = 800;
        q.reward.xp = {{Skill::Prayer, 1200}, {Skill::Defence, 1000}, {Skill::Hitpoints, 500}};
        q.reward.items = {{Item::UncutDiamond, 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "hounds_of_the_abyss";
        q.name = "Hounds of the Abyss";
        q.giver = "king";
        q.intro = "Hellhounds hunt in the Abyss and a black knight rides with them. Slay two hounds and the knight, and I will give you adamant, the finest metal in my vault.";
        q.after = "into_the_abyss";
        q.minCombat = 65;
        q.steps = {Kill("Slay 2 hellhounds in the Abyss", Species::Hellhound, 2),
                   Kill("Defeat a black knight", Species::BlackKnight, 1),
                   Talk("Report to the King", "king", "The hounds and the knight fallen! My vault is yours.")};
        q.outro = "Make something worthy of a hero. Dunmore in Millbrook can get you adamant ore now.";
        q.reward.coins = 8000;
        q.reward.xp = {{Skill::Attack, 5000}, {Skill::Strength, 5000}, {Skill::Defence, 5000}, {Skill::Hitpoints, 5000}};
        q.reward.items = {{Item::AdamantBar, 4}};
        add(q);
    }

    {   // ---- round 10: quests with a "use item" step, and quests that ask for what the new skills make
        QuestInfo q;
        q.key = "the_buried_chest";
        q.name = "The Buried Chest";
        q.giver = "gerrant";
        q.intro = "Old Captain Redbeard buried his logbook on the Lakeside shore, and sealed the key to his strongbox in the Castle Crypt. I am too old to dig. Take a spade (the general stores sell them) and dig on the shore: use the spade, then tap the ground.";
        q.minCombat = 15;
        q.steps = {Use("Dig on the Lakeside shore: use a spade there", Item::Spade, "Lakeside", false, Item::RustyKey,
                       "You dig where the old map says. The spade strikes something hard: a small rusty key!"),
                   Use("Use the rusty key in the Castle Crypt", Item::RustyKey, "Castle Crypt", true, Item::CaptainsLog,
                       "The key turns in a lock hidden in the crypt wall. Behind it lies a water-stained logbook."),
                   Have("Bring the captain's log to Gerrant", "gerrant", Item::CaptainsLog, 1, "Redbeard's own hand! Every cove and shoal he ever fished. Thank you.")};
        q.outro = "The best fishing holes in the world, in one little book.";
        q.reward.coins = 1500;
        q.reward.xp = {{Skill::Fishing, 1800}, {Skill::Crafting, 500}};
        q.reward.items = {{Item::Sapphire, 2}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "light_the_way";
        q.name = "Light the Way";
        q.giver = "priest";
        q.intro = "Travellers die in the dark places. Light three candles, one in my chapel, one in the Castle Crypt and one on the Lakeside shore, so the lost can find their way. Buy three candles and carry a tinderbox: use a candle, then tap the ground.";
        q.minCombat = 20;
        q.steps = {Use("Light a candle in the Millbrook Chapel", Item::Candle, "Millbrook Chapel", true, Item::None,
                       "The little flame steadies. The chapel feels warmer.", Item::Tinderbox),
                   Use("Light a candle in the Castle Crypt", Item::Candle, "Castle Crypt", true, Item::None,
                       "The flame pushes back the dark. Something clatters in the shadows.", Item::Tinderbox),
                   Use("Light a candle at Lakeside", Item::Candle, "Lakeside", true, Item::None,
                       "The candle burns on the shore, a tiny beacon over the water.", Item::Tinderbox),
                   Talk("Tell Father Eamon", "priest", "Three lights burning. May the lost find them. Bless you.")};
        q.outro = "Fire is the oldest kindness there is.";
        q.reward.coins = 700;
        q.reward.xp = {{Skill::Prayer, 900}, {Skill::Firemaking, 1400}};
        q.reward.items = {{Item::GoldAmulet, 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "hildas_feast";
        q.name = "Hilda's Feast";
        q.giver = "hilda";
        q.intro = "The king himself is dining in Millbrook! I need two meat pies, two garden pies and a proper cake. Bake them yourself: flour and water make dough, a pie dish holds the filling.";
        q.after = "hildas_order";
        q.reqSkill = Skill::Cooking;
        q.reqLevel = 34;
        q.steps = {Have("Bring 2 meat pies to Hilda", "hilda", Item::MeatPie, 2, "Golden crust! Just as it should be."),
                   Have("Bring 2 garden pies to Hilda", "hilda", Item::GardenPie, 2, "Tomatoes and onions, perfectly baked."),
                   Have("Bring a cake to Hilda", "hilda", Item::Cake, 1, "A masterpiece. You could cook at the palace!")};
        q.outro = "The king asked who made the cake. I said you did.";
        q.reward.coins = 900;
        q.reward.xp = {{Skill::Cooking, 4000}};
        q.reward.items = {{Item::RedTopaz, 2}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "bowyers_pride";
        q.name = "The Bowyer's Pride";
        q.giver = "larson";
        q.intro = "A bowyer is only as good as his bows, and I have forgotten how to carve. Show me what you can do: bring an oak longbow and a yew longbow that you made yourself. Carve the logs with a knife, then string them.";
        q.reqSkill = Skill::Woodcutting;
        q.reqLevel = 25;
        q.steps = {Have("Bring an oak longbow to Larson", "larson", Item::OakLongbow, 1, "Fine grain, true curve. Now the real test."),
                   Have("Bring a yew longbow to Larson", "larson", Item::YewLongbow, 1, "Yew! I have not held a bow like this in years.")};
        q.outro = "You have a bowyer's hands. Take these, you will need them. I will sell you mithril arrows too.";
        q.reward.coins = 2000;
        q.reward.xp = {{Skill::Ranged, 3000}, {Skill::Woodcutting, 2500}};
        q.reward.items = {{Item::MithrilArrows, 100}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "a_knights_kit";
        q.name = "A Knight's Kit";
        q.giver = "brannock";
        q.intro = "A young knight needs a kit by the end of the month and I have too many orders. Forge him a steel scimitar, a steel full helm and a steel chainbody. Steel bars are made from iron ore and coal at a furnace.";
        q.reqSkill = Skill::Smithing;
        q.reqLevel = 36;
        q.steps = {Have("Bring a steel scimitar to Brannock", "brannock", GearItem(Tier::Steel, GearType::Scimitar), 1, "A clean edge. He will like that."),
                   Have("Bring a steel full helm to Brannock", "brannock", GearItem(Tier::Steel, GearType::Fullhelm), 1, "Good, solid work."),
                   Have("Bring a steel chainbody to Brannock", "brannock", GearItem(Tier::Steel, GearType::Chainbody), 1, "Not a link out of place. You have a smith's eye.")};
        q.outro = "Here, for your trouble, and a little extra for the finish. Mira and I will stock mithril for you now.";
        q.reward.coins = 2500;
        q.reward.xp = {{Skill::Smithing, 4500}};
        q.reward.items = {{Item::MithrilBar, 5}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "ferrymans_feast";
        q.name = "The Ferryman's Feast";
        q.giver = "silas";
        q.intro = "Tonight is my daughter's wedding, and a ferryman's wedding wants a sea feast. Three lobsters, two bass and a shark, all cooked. Do that and I will never charge you for the crossing again. (I will, but I will mean it kindly.)";
        q.after = "the_big_one";
        q.reqSkill = Skill::Fishing;
        q.reqLevel = 46;
        q.steps = {Have("Bring 3 lobsters to Silas", "silas", Item::Lobster, 3, "Big, red and beautiful."),
                   Have("Bring 2 bass to Silas", "silas", Item::Bass, 2, "These will go down well."),
                   Have("Bring a shark to Silas", "silas", Item::Shark, 1, "A shark! The whole village will talk about this wedding.")};
        q.outro = "You have made an old man very happy.";
        q.reward.coins = 3000;
        q.reward.xp = {{Skill::Fishing, 4000}, {Skill::Cooking, 2500}};
        q.reward.items = {{Item::DiamondRing, 1}};
        add(q);
    }
    // ---- round 12: errands for the wider world (Elmswick, Saltmere, Sunfield Farm, Northwatch, the Standing Stones, the swamp and the downs)
    {
        QuestInfo q;
        q.key = "hobbs_oak_order";
        q.name = "Hobb's Oak Order";
        q.giver = "hobb";
        q.intro = "A cartwright in the next valley wants twelve good oak logs, and I have a dozen other jobs. Cut them for me - oak, mind, not elm. You will need a woodcutting level of fifteen.";
        q.reqSkill = Skill::Woodcutting;
        q.reqLevel = 15;
        q.steps = {Have("Bring 12 oak logs to Hobb", "hobb", Item::OakLogs, 12, "Straight grain, no rot. The cartwright will be glad.")};
        q.outro = "A fair day's work. Here is your share, and a bit more for the clean cuts.";
        q.reward.coins = 450;
        q.reward.xp = {{Skill::Woodcutting, 1400}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "wolves_of_the_pines";
        q.name = "Wolves of the Pines";
        q.giver = "perrin";
        q.intro = "The wolves have grown bold in Whispering Pines - they took a calf from the edge of the village last night. Kill five of them in the pines north and west of Elmswick and the woodcutters will sleep again.";
        q.minCombat = 10;
        q.steps = {Kill("Kill 5 wolves in the pines", Species::Wolf, 5),
                   Talk("Tell Perrin", "perrin", "Five fewer wolves! The woodcutters will cheer tonight.")};
        q.outro = "Take this for your trouble. And come back for logs any time.";
        q.reward.coins = 600;
        q.reward.xp = {{Skill::Attack, 700}, {Skill::Strength, 700}, {Skill::Hitpoints, 500}};
        q.reward.items = {{G(Tier::Steel, GearType::Helm), 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "saltmeres_catch";
        q.name = "Saltmere's Catch";
        q.giver = "brine";
        q.intro = "Market day is tomorrow and my stall is half empty. Bring me six cooked mackerel and four cooked cod - catch them off the pier, cook them well - and I will see you right.";
        q.reqSkill = Skill::Fishing;
        q.reqLevel = 16;
        q.steps = {Have("Bring 6 mackerel to Brine", "brine", Item::Mackerel, 6, "Golden skin, firm flesh. Beautiful."),
                   Have("Bring 4 cod to Brine", "brine", Item::Cod, 4, "Fat and flaky. The whole market will queue for these.")};
        q.outro = "You have saved my market day. Here, buy yourself something nice.";
        q.reward.coins = 800;
        q.reward.xp = {{Skill::Fishing, 1800}, {Skill::Cooking, 1200}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "farm_pests";
        q.name = "Farm Pests";
        q.giver = "ada";
        q.intro = "The moles have got into the Sunfields again - great clawed things, they dig up the whole turnip field. Kill five of them in the fields around the farm, and bring me a few eggs for the dinner while you are about it.";
        q.minCombat = 12;
        q.steps = {Kill("Kill 5 giant moles in the Sunfields", Species::GiantMole, 5),
                   Have("Bring 4 eggs to Ada", "ada", Item::Egg, 4, "Still warm. You have made a farmer very happy.")};
        q.outro = "Here is something for your trouble. The turnips will be safe until spring.";
        q.reward.coins = 700;
        q.reward.xp = {{Skill::Attack, 900}, {Skill::Defence, 900}, {Skill::Hitpoints, 500}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "northwatch_vigil";
        q.name = "The Northwatch Vigil";
        q.giver = "watchman";
        q.intro = "I keep the lamp lit up here, but the dark wolves come down from the peaks at night and I cannot leave my post. Cull four of them on the Northern Reach and I will tell the castle you are to be trusted.";
        q.minCombat = 18;
        q.steps = {Kill("Kill 4 dark wolves on the Northern Reach", Species::DarkWolf, 4),
                   Talk("Tell Watchman Corwin", "watchman", "Four dark wolves. The lamp will burn easier tonight.")};
        q.outro = "A hard walk, and you made it. Take this, it has kept me warm for many winters.";
        q.reward.coins = 1100;
        q.reward.xp = {{Skill::Attack, 1500}, {Skill::Strength, 1500}, {Skill::Hitpoints, 1000}};
        q.reward.items = {{G(Tier::Steel, GearType::Fullhelm), 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "bandits_in_the_hollow";
        q.name = "Bandits in the Hollow";
        q.giver = "captain";
        q.intro = "Bandits have been stopping the carts on the Darkwood road. Their camp is in a hollow deep in the wood, east of the Sunfields. Find it, deal with five of them, and bring me word.";
        q.minCombat = 25;
        q.steps = {Reach("Find the bandits' hollow in Darkwood", "Bandits' Hollow"),
                   Kill("Kill 5 bandits", Species::Bandit, 5),
                   Talk("Tell Captain Redd", "captain", "Five bandits down. The carts will roll again.")};
        q.outro = "Saltmere owes you. Take this, it was taken from a smuggler years ago.";
        q.reward.coins = 1600;
        q.reward.xp = {{Skill::Attack, 2200}, {Skill::Strength, 2200}, {Skill::Defence, 2200}, {Skill::Hitpoints, 1500}};
        q.reward.items = {{Item::SapphireRing, 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "giants_and_stones";
        q.name = "Giants and Stones";
        q.giver = "druid";
        q.intro = "The hill giants have been tearing at the old stones, and the stones are all that hold this place together. Drive three of them off - they roam the downs to the east and the foothills to the north - and the circle will hold another thousand years.";
        q.after = "bandits_in_the_hollow";
        q.minCombat = 40;
        q.steps = {Kill("Kill 3 hill giants", Species::HillGiant, 3),
                   Talk("Tell Brother Elwyn", "druid", "Three giants. The stones can rest.")};
        q.outro = "You walk softly and carry a heavy stick. Take this, you have earned it.";
        q.reward.coins = 3000;
        q.reward.xp = {{Skill::Attack, 3500}, {Skill::Strength, 3500}, {Skill::Defence, 3500}, {Skill::Prayer, 1000}};
        q.reward.items = {{Item::DiamondAmulet, 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "maggies_brew";
        q.name = "Maggie's Brew";
        q.giver = "maggie";
        q.intro = "Hee hee. I need things for the pot, dearie: two candles, four redberries, five bones. No questions. A brew wants what a brew wants.";
        q.steps = {Have("Bring 2 candles to Maggie", "maggie", Item::Candle, 2, "Wax. Good. Now the berries."),
                   Have("Bring 4 redberries to Maggie", "maggie", Item::Redberries, 4, "Red as sin. Now the bones."),
                   Have("Bring 5 bones to Maggie", "maggie", Item::Bones, 5, "Hee hee, it is going to be a good one.")};
        q.outro = "Take a spoonful... no? Take some runes, then. They are safer.";
        q.reward.coins = 400;
        q.reward.xp = {{Skill::Magic, 1500}, {Skill::Prayer, 500}};
        q.reward.items = {{Item::ChaosRune, 20}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "restless_barrows";
        q.name = "The Restless Barrows";
        q.giver = "maggie";
        q.intro = "The dead walk the downs to the south, and the old bones are angry. Put three of the skeleton warriors back in their mounds, dearie, and I will pay in something that glitters.";
        q.after = "maggies_brew";
        q.minCombat = 30;
        q.steps = {Kill("Kill 3 skeleton warriors on the Barrow Downs", Species::SkeletonWarrior, 3),
                   Talk("Tell Old Maggie", "maggie", "Quiet at last! Hee hee, the mounds will keep.")};
        q.outro = "A glitter for a bone. Fair is fair.";
        q.reward.coins = 1800;
        q.reward.xp = {{Skill::Attack, 2600}, {Skill::Strength, 2600}, {Skill::Hitpoints, 1800}, {Skill::Prayer, 1500}};
        q.reward.items = {{Item::Emerald, 3}};
        add(q);
    }
    // ---- round 13: more errands for the wild country (the places and the people are the ones the wider world already has)
    {
        QuestInfo q;
        q.key = "greenmarch_survey";
        q.name = "The Greenmarch Survey";
        q.giver = "perrin";
        q.intro = "The woodcutters want new ground, and nobody has walked the country south of the pines in years. Go and look at Greenmarch and then Elmwood, and tell me whether it is fit for felling.";
        q.minCombat = 10;
        q.steps = {Reach("Walk into Greenmarch", "Greenmarch"), Reach("Walk into Elmwood", "Elmwood"),
                   Talk("Report to Perrin", "perrin", "Good grass, good timber, and no wolves worth the name. We will plan a camp in the spring.")};
        q.outro = "That is a long walk well spent. Here is something for your boots.";
        q.reward.coins = 500;
        q.reward.xp = {{Skill::Woodcutting, 900}, {Skill::Hitpoints, 400}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "teak_for_the_cartwright";
        q.name = "Teak for the Cartwright";
        q.giver = "hobb";
        q.after = "hobbs_oak_order";
        q.reqSkill = Skill::Woodcutting;
        q.reqLevel = 35;
        q.intro = "The cartwright liked your oak. Now he wants teak for the wheel hubs: ten logs of it, and not a split one among them. You need a woodcutting level of thirty-five.";
        q.steps = {Have("Bring 10 teak logs to Hobb", "hobb", Item::TeakLogs, 10, "Dense and red. Those hubs will outlast the cart.")};
        q.outro = "Take the axe he sent in payment: it is better than anything I have on the shelf.";
        q.reward.coins = 1200;
        q.reward.xp = {{Skill::Woodcutting, 3200}};
        q.reward.items = {{G(Tier::Mithril, GearType::Hatchet), 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "deep_water_haul";
        q.name = "The Deep Water Haul";
        q.giver = "brine";
        q.after = "saltmeres_catch";
        q.reqSkill = Skill::Fishing;
        q.reqLevel = 35;
        q.intro = "The inn at the harbour wants something grander than cod. Bring me five lobsters and four tuna - the deep fish, the kind you take from a cage and a harpoon - and I will pay what they are worth.";
        q.steps = {Have("Bring 5 lobsters to Brine", "brine", Item::Lobster, 5, "Red as sunset. The inn will be delighted."),
                   Have("Bring 4 tuna to Brine", "brine", Item::Tuna, 4, "Heavy, clean fish. You have the hands for the deep water.")};
        q.outro = "The harbour has a new favourite fisher. This is for you, and so are these.";
        q.reward.coins = 2000;
        q.reward.xp = {{Skill::Fishing, 3600}, {Skill::Cooking, 1500}};
        q.reward.items = {{Item::SeaTurtle, 3}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "iron_range_run";
        q.name = "The Iron Range Run";
        q.giver = "captain";
        q.after = "bandits_in_the_hollow";
        q.minCombat = 25;
        q.intro = "Saltmere's smiths are out of iron, and the carts will not go through Darkwood while the bandits are about - so I need someone who can walk. Go to the Iron Range, dig twelve iron ore and six coal, and bring them to me.";
        q.steps = {Reach("Walk into the Iron Range", "The Iron Range"), Have("Bring 12 iron ore to Captain Redd", "captain", Item::IronOre, 12, "Good ore, and plenty of it."),
                   Have("Bring 6 coal to Captain Redd", "captain", Item::Coal, 6, "That will keep the forges hot until the roads are safe again.")};
        q.outro = "The smiths will be hammering for a month. Take this chainbody: a smith made it for the watch.";
        q.reward.coins = 1800;
        q.reward.xp = {{Skill::Mining, 3000}, {Skill::Smithing, 1200}};
        q.reward.items = {{G(Tier::Steel, GearType::Chainbody), 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "cold_comfort";
        q.name = "Cold Comfort";
        q.giver = "watchman";
        q.after = "northwatch_vigil";
        q.minCombat = 18;
        q.intro = "The wind on the moor goes through a coat like it was paper. A hot stew is the only thing that gets the cold out of my bones. Look at the moor for yourself, then bring me four stews and I will make it worth the walk.";
        q.steps = {Reach("Walk onto Northwatch Moor", "Northwatch Moor"),
                   Have("Bring 4 stews to Watchman Corwin", "watchman", Item::Stew, 4, "Hot, thick and salty. A man could stand a whole winter on this.")};
        q.outro = "You have kept a lamp-keeper alive. Take this: it is warmer than it looks.";
        q.reward.coins = 1400;
        q.reward.xp = {{Skill::Cooking, 2600}, {Skill::Defence, 1200}};
        q.reward.items = {{G(Tier::Mithril, GearType::Helm), 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "night_at_the_stones";
        q.name = "A Night at the Stones";
        q.giver = "druid";
        q.after = "giants_and_stones";
        q.minCombat = 40;
        q.intro = "The circle wants a vigil. Walk out to the Dune Sea and see what the sand has uncovered, then come back and light a candle among the stones at dusk. I will watch with you.";
        q.steps = {Reach("Walk into the Dune Sea", "The Dune Sea"),
                   Use("Light a candle at the Standing Stones", Item::Candle, "The Standing Stones", true, Item::None,
                       "The flame stands straight up in the still air, and for a moment the stones seem to hum.", Item::Tinderbox),
                   Talk("Tell Brother Elwyn", "druid", "I felt it too. The old powers are settled. Thank you.")};
        q.outro = "Few people give a night to the stones. Take these gifts of the downs.";
        q.reward.coins = 3500;
        q.reward.xp = {{Skill::Prayer, 3500}, {Skill::Defence, 2500}, {Skill::Hitpoints, 1500}};
        q.reward.items = {{Item::Diamond, 2}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "harvest_help";
        q.name = "Harvest Help";
        q.giver = "ada";
        q.intro = "Harvest is upon us and my hands are not enough. Bring me twelve potatoes and eight onions for the cellar, and five pots of flour for the baking, and the farm will pay you well.";
        q.steps = {Have("Bring 12 potatoes to Ada", "ada", Item::Potato, 12, "Clean and firm. Into the cellar they go."),
                   Have("Bring 8 onions to Ada", "ada", Item::Onion, 8, "Sharp enough to make a farmer cry."),
                   Have("Bring 5 flour to Ada", "ada", Item::Flour, 5, "Fine as snow. The baking will be grand.")};
        q.outro = "That is the harvest in. Take this for your trouble, and come to the farm any time.";
        q.reward.coins = 600;
        q.reward.xp = {{Skill::Cooking, 900}, {Skill::Crafting, 400}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "witchs_errand";
        q.name = "The Witch's Errand";
        q.giver = "maggie";
        q.after = "restless_barrows";
        q.minCombat = 30;
        q.intro = "There is a sourness in the Mirewood, dearie: the swamp-dead are up and walking, and they will spoil the whole brew. Go and look, put four zombies to rest, and come tell old Maggie about it.";
        q.steps = {Reach("Walk into Mirewood Swamp", "Mirewood Swamp"), Kill("Kill 4 zombies in the swamp", Species::Zombie, 4),
                   Talk("Tell Old Maggie", "maggie", "Four fewer to stir the mud. Hee hee, the brew will taste cleaner now.")};
        q.outro = "A witch pays her debts. Here are runes, and a little of my own luck.";
        q.reward.coins = 2200;
        q.reward.xp = {{Skill::Magic, 3000}, {Skill::Prayer, 2500}, {Skill::Attack, 2000}, {Skill::Hitpoints, 1500}};
        q.reward.items = {{Item::ChaosRune, 40}};
        add(q);
    }
    // ---- round 13: the Abyss. The best resources in the game lie there (adamant and gem rocks, mithril, magic and yew trees), and so do the other
    // players who hunt for them: these are the late-game errands. Whatever you carry there, a skull makes you lose it all.
    {
        QuestInfo q;
        q.key = "riches_of_the_abyss";
        q.name = "The Riches of the Abyss";
        q.giver = "king";
        q.after = "hounds_of_the_abyss";
        q.minCombat = 65;
        q.reqSkill = Skill::Mining;
        q.reqLevel = 70;
        q.intro = "The Abyss is a grave for the careless, but its rocks hold what no other mine in the realm does: adamant, in plenty. The crown needs six pieces of the ore. Dig it yourself (you will need a mining level of seventy), and keep your wits: others hunt there, and a skull on your head means you lose everything you carry if you fall.";
        q.steps = {Reach("Enter the Abyss", "The Abyss"), Have("Bring 6 adamant ore to the King", "king", Item::AdamantOre, 6, "Adamant, fresh from the dark. The smiths will weep for joy.")};
        q.outro = "The realm owes you a great deal. Take this helm: it has stood in our vault for a century.";
        q.reward.coins = 8000;
        q.reward.xp = {{Skill::Mining, 6000}, {Skill::Smithing, 3000}, {Skill::Defence, 2500}};
        q.reward.items = {{G(Tier::Adamant, GearType::Fullhelm), 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "heart_of_the_dark";
        q.name = "The Heart of the Dark";
        q.giver = "aldric";
        q.after = "dark_magic";
        q.minCombat = 55;
        q.reqSkill = Skill::Woodcutting;
        q.reqLevel = 75;
        q.intro = "Magic trees grow in the Abyss, nourished by something old and bad. Their logs make the finest staves there are. Cut me six - a woodcutting level of seventy-five, mind - and I will teach your hands what your head already knows.";
        q.steps = {Reach("Enter the Abyss", "The Abyss"), Have("Bring 6 magic logs to Aldric", "aldric", Item::MagicLogs, 6, "They hum in my hands. Wonderful. Wonderful!")};
        q.outro = "Study these well. Runes, and what little I have left to teach.";
        q.reward.coins = 5000;
        q.reward.xp = {{Skill::Magic, 9000}, {Skill::Woodcutting, 6000}, {Skill::Firemaking, 2000}};
        q.reward.items = {{Item::FireRune, 200}, {Item::ChaosRune, 100}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "dark_pilgrims";
        q.name = "The Dark Pilgrims";
        q.giver = "priest";
        q.after = "scorpion_stings";
        q.minCombat = 70;
        q.intro = "Travellers who go to the Abyss and do not come home leave their bones for the black knights and the hellhounds that guard its depths. Put four hellhounds and three black knights to rest, so the pilgrims' road to the old shrine is safe once more.";
        q.steps = {Reach("Enter the Abyss", "The Abyss"), Kill("Kill 4 hellhounds in the Abyss", Species::Hellhound, 4),
                   Kill("Kill 3 black knights in the Abyss", Species::BlackKnight, 3), Talk("Tell the priest", "priest", "The shrine road is quiet. Bless you, brave soul.")};
        q.outro = "May the light go with you into dark places.";
        q.reward.coins = 6000;
        q.reward.xp = {{Skill::Prayer, 6000}, {Skill::Attack, 3500}, {Skill::Strength, 3500}, {Skill::Hitpoints, 3000}};
        q.reward.items = {{Item::DragonstoneAmulet, 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "gems_for_the_crown";
        q.name = "Gems for the Crown";
        q.giver = "king";
        q.after = "riches_of_the_abyss";
        q.minCombat = 70;
        q.intro = "My crown lost its stones in the war. The Abyss has gem rocks too, if you can bear the dark. Bring me four diamonds (cut them at the jeweller's bench) and the crown shall shine again.";
        q.steps = {Have("Bring 4 diamonds to the King", "king", Item::Diamond, 4, "Flawless. You have a jeweller's eye as well as a warrior's arm.")};
        q.outro = "A king remembers his friends. This amulet is the finest in the treasury.";
        q.reward.coins = 12000;
        q.reward.xp = {{Skill::Crafting, 6000}, {Skill::Mining, 3000}, {Skill::Defence, 3000}};
        q.reward.items = {{Item::DragonstoneAmulet, 1}};
        add(q);
    }
    {
        QuestInfo q;
        q.key = "the_black_dragon";
        q.name = "The Black Dragon";
        q.giver = "king";
        q.after = "gems_for_the_crown";
        q.minCombat = 80;
        q.intro = "Deep in the Abyss a black dragon sleeps on a heap of the dead. It has gone unchallenged for a hundred years. Slay it, and bring me word - but know that you will not be alone down there, and that the dragon's hoard draws killers.";
        q.steps = {Kill("Slay the black dragon in the Abyss", Species::BlackDragon, 1), Talk("Tell the King", "king", "The black dragon, dead? Then you are the greatest hero this realm has seen.")};
        q.outro = "There is nothing left in my treasury worthy of you, so take the best armour my smiths ever made.";
        q.reward.coins = 20000;
        q.reward.xp = {{Skill::Attack, 9000}, {Skill::Strength, 9000}, {Skill::Defence, 9000}, {Skill::Hitpoints, 7000}};
        q.reward.items = {{G(Tier::Adamant, GearType::Body), 1}};
        add(q);
    }
    return v;
}

}  // namespace

const std::vector<QuestInfo>& AllQuests() {
    static const std::vector<QuestInfo> quests = Build();
    return quests;
}

int FindQuest(const std::string& key) {
    const std::vector<QuestInfo>& q = AllQuests();
    for (size_t i = 0; i < q.size(); ++i)
        if (q[i].key == key) return static_cast<int>(i);
    return -1;
}

QuestLog::QuestLog() : states_(AllQuests().size()) {}

int QuestLog::Points() const {
    int n = 0;
    for (const QuestState& s : states_) n += s.done ? 1 : 0;
    return n;
}

bool QuestLog::CanStart(int i, const Skills& skills) const {
    if (i < 0 || i >= Count()) return false;
    return Missing(i, skills).empty() && !states_[static_cast<size_t>(i)].started && !states_[static_cast<size_t>(i)].done;
}

std::string QuestLog::Missing(int i, const Skills& skills) const {
    if (i < 0 || i >= Count()) return "That quest does not exist.";
    const QuestInfo& q = AllQuests()[static_cast<size_t>(i)];
    if (!q.after.empty()) {
        const int a = FindQuest(q.after);
        if (a >= 0 && !states_[static_cast<size_t>(a)].done) return "You need to finish \"" + AllQuests()[static_cast<size_t>(a)].name + "\" first.";
    }
    if (skills.CombatLevel() < q.minCombat) return "You need combat level " + std::to_string(q.minCombat) + ".";
    if (q.reqLevel > 0 && skills.Level(q.reqSkill) < q.reqLevel)
        return std::string("You need ") + SkillName(q.reqSkill) + " level " + std::to_string(q.reqLevel) + ".";
    if (Points() < q.minPoints) return "You need " + std::to_string(q.minPoints) + " quest points (you have " + std::to_string(Points()) + ").";
    return "";
}

void QuestLog::Start(int i) {
    if (i < 0 || i >= Count()) return;
    QuestState& s = states_[static_cast<size_t>(i)];
    if (s.started || s.done) return;
    s = QuestState{};
    s.started = true;
}

const QuestStep* QuestLog::Current(int i) const {
    if (i < 0 || i >= Count()) return nullptr;
    const QuestState& s = states_[static_cast<size_t>(i)];
    const QuestInfo& q = AllQuests()[static_cast<size_t>(i)];
    if (!s.started || s.done || s.step < 0 || s.step >= static_cast<int>(q.steps.size())) return nullptr;
    return &q.steps[static_cast<size_t>(s.step)];
}

int QuestLog::OnKill(Species sp, AttackKind kind) {
    int completed = -1;
    for (int i = 0; i < Count(); ++i) {
        const QuestStep* st = Current(i);
        if (!st || st->kind != StepKind::Kill || st->species != sp) continue;
        if (st->killKind != kAnyWeapon && st->killKind != static_cast<int>(kind)) continue;
        QuestState& s = states_[static_cast<size_t>(i)];
        if (++s.count >= st->count) {
            ++s.step;
            s.count = 0;
            completed = i;
        }
    }
    return completed;
}

int QuestLog::OnReach(const std::string& area) {
    int completed = -1;
    for (int i = 0; i < Count(); ++i) {
        const QuestStep* st = Current(i);
        if (!st || st->kind != StepKind::Reach || st->area != area) continue;
        QuestState& s = states_[static_cast<size_t>(i)];
        ++s.step;
        s.count = 0;
        completed = i;
    }
    return completed;
}

int QuestLog::FindUse(Item item, const std::string& area) const {
    for (int i = 0; i < Count(); ++i) {
        const QuestStep* st = Current(i);
        if (st && st->kind == StepKind::Use && st->item == item && st->area == area) return i;
    }
    return -1;
}

bool QuestLog::WantsItem(Item item) const {
    for (int i = 0; i < Count(); ++i) {
        const QuestStep* st = Current(i);
        if (st && st->kind == StepKind::Use && st->item == item) return true;
    }
    return false;
}

bool QuestLog::StepReady(int i, const std::string& person, const Inventory& bag) const {
    const QuestStep* st = Current(i);
    if (!st || st->person != person) return false;
    if (st->kind == StepKind::Talk) return true;
    if (st->kind == StepKind::Have) return bag.Count(st->item) >= st->count;
    return false;
}

bool QuestLog::CompleteStep(int i, Inventory* bag) {
    const QuestStep* st = Current(i);
    if (!st) return false;
    if (st->kind == StepKind::Have && st->take && bag) bag->Remove(st->item, st->count);
    if (st->kind == StepKind::Use && st->take && bag) bag->Remove(st->item, 1);
    QuestState& s = states_[static_cast<size_t>(i)];
    ++s.step;
    s.count = 0;
    return s.step >= static_cast<int>(AllQuests()[static_cast<size_t>(i)].steps.size());
}

void QuestLog::Finish(int i) {
    if (i < 0 || i >= Count()) return;
    QuestState& s = states_[static_cast<size_t>(i)];
    s.done = true;
    s.started = true;
    s.step = static_cast<int>(AllQuests()[static_cast<size_t>(i)].steps.size());
}

std::string QuestLog::ObjectiveText(int i, const Inventory& bag) const {
    const QuestStep* st = Current(i);
    if (!st) return "";
    std::string t = st->objective;
    const QuestState& s = states_[static_cast<size_t>(i)];
    if (st->kind == StepKind::Kill && st->count > 1) t += " (" + std::to_string(s.count) + "/" + std::to_string(st->count) + ")";
    if (st->kind == StepKind::Have) t += " (you have " + std::to_string(std::min(bag.Count(st->item), 9999)) + ")";
    return t;
}

std::string QuestLog::Serialize() const {
    std::ostringstream o;
    for (int i = 0; i < Count(); ++i) {
        const QuestState& s = states_[static_cast<size_t>(i)];
        if (!s.started && !s.done) continue;
        o << "quest." << AllQuests()[static_cast<size_t>(i)].key << "=" << s.step << ":" << s.count << ":" << (s.done ? 1 : 0) << "\n";
    }
    return o.str();
}

void QuestLog::ParseLine(const std::string& key, const std::string& value) {
    const int i = FindQuest(key);
    if (i < 0) return;
    const size_t c1 = value.find(':');
    const size_t c2 = c1 == std::string::npos ? c1 : value.find(':', c1 + 1);
    if (c2 == std::string::npos) return;
    QuestState& s = states_[static_cast<size_t>(i)];
    const int steps = static_cast<int>(AllQuests()[static_cast<size_t>(i)].steps.size());
    s.started = true;
    s.step = std::max(0, std::min(std::atoi(value.c_str()), steps));
    s.count = std::max(0, std::atoi(value.c_str() + c1 + 1));
    s.done = std::atoi(value.c_str() + c2 + 1) != 0;
    if (s.done) s.step = steps;
}

}  // namespace journey
