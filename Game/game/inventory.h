#pragma once

#include <vector>
#include <memory>

#include <daedalus/DaedalusGameState.h>

class Item;
class WorldScript;
class Npc;

class Inventory final {
  public:
    Inventory();
    ~Inventory();

    enum Flags : uint32_t {
      ITM_CAT_NONE   = 1 << 0,
      ITM_CAT_NF     = 1 << 1,
      ITM_CAT_FF     = 1 << 2,
      ITM_CAT_MUN    = 1 << 3,
      ITM_CAT_ARMOR  = 1 << 4,
      ITM_CAT_FOOD   = 1 << 5,
      ITM_CAT_DOCS   = 1 << 6,
      ITM_CAT_POTION = 1 << 7,
      ITM_CAT_LIGHT  = 1 << 8,
      ITM_CAT_RUNE   = 1 << 9,
      ITM_10         = 1 << 10, // ???
      ITM_RING       = 1 << 11,
      ITM_MISSION    = 1 << 12,
      ITM_DAG        = 1 << 13,
      ITM_SWD        = 1 << 14,
      ITM_AXE        = 1 << 15,
      ITM_2HD_SWD    = 1 << 16,
      ITM_2HD_AXE    = 1 << 17,
      ITM_SHIELD     = 1 << 18,
      ITM_BOW        = 1 << 19,
      ITM_CROSSBOW   = 1 << 20,
      ITM_MULTI      = 1 << 21,
      ITM_AMULET     = 1 << 22,
      ITM_BELT       = 1 << 24,
      };

    size_t goldCount() const;
    size_t itemCount(const size_t id) const;
    size_t recordsCount() const { return items.size(); }
    const Item& at(size_t i) const;

    void   addItem(std::unique_ptr<Item>&& p,  WorldScript& vm, Npc &owner);
    void   addItem(size_t cls, uint32_t count, WorldScript& vm, Npc &owner);
    void   delItem(size_t cls, uint32_t count, WorldScript& vm, Npc &owner);
    bool   use    (size_t cls, WorldScript &vm, Npc &owner);
    bool   unequip(size_t cls, WorldScript &vm, Npc &owner);
    void   unequip(Item*  cls, WorldScript &vm, Npc &owner);
    void   invalidateCond();
    bool   isChanged() const { return ch; }
    void   autoEquip(WorldScript &vm, Npc &owner);
    void   updateArmourView(WorldScript &vm, Npc& owner);

  private:
    bool   setSlot     (Item*& slot, Item *next, WorldScript &vm, Npc &owner);
    bool   equipNumSlot(Item *next, WorldScript &vm, Npc &owner);
    void   applyArmour (Item& it, WorldScript &vm, Npc &owner, int32_t sgn);

    Item*  findByClass(size_t cls);
    void   delItem    (Item* it, uint32_t count, WorldScript& vm, Npc& owner);
    Item*  bestArmour (WorldScript &vm, Npc &owner);

    std::vector<std::unique_ptr<Item>> items;
    Item*                              armour=nullptr;
    Item*                              belt  =nullptr;
    Item*                              amulet=nullptr;
    Item*                              ringL =nullptr;
    Item*                              ringR =nullptr;

    Item*                              mele  =nullptr;
    Item*                              range =nullptr;
    Item*                              numslot[8]={};
    bool                               ch=false;
  };