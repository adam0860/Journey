// Human characters who stand about the world: shopkeepers, smiths, villagers. Pure C++ (unit tested).
//
// The map (`@person key x z yaw`) says where each one stands; the table in People.cpp says who they are: name, what they
// look like, what they wear, whether they run a shop, and what they say. They are drawn with the same modular character
// as the hero (see main.cpp), so they cost no extra models.
#pragma once
#include <string>
#include <vector>

#include "game/Inventory.h"
#include "world/TileMap.h"

namespace journey {

// How a person looks, as indices into the character creator's choices (hairPick = which of the hairstyles open to their sex).
struct PersonLook {
    bool female = false;
    int skin = 2, hairPick = 0, hairColor = 2;
    int shirt = 0, shirtColor = 7, pants = 0, pantsColor = 3, shoes = 1, shoesColor = 0;
};

enum class PersonWork { Idle, Smith };   // Smith: hammers on an anvil all day

struct PersonInfo {
    const char* key;        // "aubrey" (map file)
    const char* name;       // "Aubrey"
    const char* title;      // "Shopkeeper"  (shown in examine and the shop window)
    const char* examine;
    const char* shop;       // key of the shop they run ("" = none)
    int reach;              // how far across a counter you can still talk to them (1 = next to them)
    PersonLook look;
    PersonWork work;
    Item weapon, shield, helm, body, legs;   // what they wear (Item::None = nothing)
    int talkCount;
    const char* talk[3];    // things they say (the shopkeepers greet you with the first)
};

const PersonInfo* FindPersonInfo(const std::string& key);
const std::vector<PersonInfo>& AllPersonInfos();

struct Person {
    int id = 0;
    const PersonInfo* info = nullptr;
    float x = 0, z = 0;
    float facing = 0;        // radians, 0 = facing +z
    int nextLine = 0;        // which of their sayings comes next
    TileCoord Tile() const { return {static_cast<int>(x), static_cast<int>(z)}; }
};

class People {
public:
    void Init(const TileMap& map);
    const std::vector<Person>& All() const { return people_; }
    std::vector<Person>& All() { return people_; }
    const Person* Find(int id) const;
    Person* Find(int id);
    int AtTile(TileCoord t) const;   // id of the person standing on this tile, or -1

private:
    std::vector<Person> people_;
};

}  // namespace journey
