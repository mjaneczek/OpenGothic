#include "gamemenu.h"

#include <Tempest/Painter>
#include <Tempest/Log>

#include "world/world.h"
#include "ui/menuroot.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

static const float scriptDiv=8192.0f;

GameMenu::GameMenu(MenuRoot &owner, Gothic &gothic, const char* menuSection)
  :gothic(gothic), owner(owner) {
  timer.timeout.bind(this,&GameMenu::onTick);
  timer.start(100);

  textBuf.reserve(64);
  vm = gothic.createVm("_work/Data/Scripts/_compiled/MENU.DAT");

  Daedalus::DATFile& dat=vm->getDATFile();

  hMenu = vm->getGameState().createMenu();
  vm->initializeInstance(ZMemory::toBigHandle(hMenu),
                         dat.getSymbolIndexByName(menuSection),
                         Daedalus::IC_Menu);

  Daedalus::GEngineClasses::C_Menu& menu = vm->getGameState().getMenu(hMenu);

  back = Resources::loadTexture(menu.backPic);
  //back = Resources::loadTexture("menu_gothicshadow.tga");
  //back = Resources::loadTexture("menu_gothic.tga");

  initItems();
  if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_SHOW_INFO) {
    float infoX = 1000.0f/scriptDiv;
    float infoY = 7500.0f/scriptDiv;

    // There could be script-defined values
    if(dat.hasSymbolName("MENU_INFO_X") && dat.hasSymbolName("MENU_INFO_X")) {
      Daedalus::PARSymbol& symX = dat.getSymbolByName("MENU_INFO_X");
      Daedalus::PARSymbol& symY = dat.getSymbolByName("MENU_INFO_Y");

      infoX = symX.getInt()/scriptDiv;
      infoY = symY.getInt()/scriptDiv;
      }
    setPosition(int(infoX*w()),int(infoY*h()));
    }

  setSelection(gothic.isInGame() ? menu.defaultInGame : menu.defaultOutGame);
  initValues();

  gothic.pushPause();
  }

GameMenu::~GameMenu() {
  gothic.popPause();
  vm->getGameState().removeMenu(hMenu);
  }

void GameMenu::initItems() {
  Daedalus::GEngineClasses::C_Menu& menu = vm->getGameState().getMenu(hMenu);

  for(int i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;++i){
    if(menu.items[i].empty())
      continue;

    hItems[i].name   = menu.items[i];
    hItems[i].handle = vm->getGameState().createMenuItem();
    vm->initializeInstance(ZMemory::toBigHandle(hItems[i].handle),
                           vm->getDATFile().getSymbolIndexByName(hItems[i].name),
                           Daedalus::IC_MenuItem);
    auto& item=hItems[i].get(*vm);
    hItems[i].img = Resources::loadTexture(item.backPic);
    }
  }

void GameMenu::paintEvent(PaintEvent &e) {
  Painter p(e);
  if(back) {
    p.setBrush(*back);
    p.drawRect(0,0,w(),h(),
               0,0,back->w(),back->h());
    }

  for(auto& hItem:hItems){
    if(!hItem.handle.isValid())
      continue;
    Daedalus::GEngineClasses::C_Menu_Item&        item = hItem.get(*vm);
    Daedalus::GEngineClasses::C_Menu_Item::EFlags flags=Daedalus::GEngineClasses::C_Menu_Item::EFlags(item.flags);
    getText(item,textBuf);

    Color clText = clNormal;
    if(item.fontName=="font_old_10_white.tga"){
      p.setFont(Resources::font());
      clText = clNormal;
      }
    else if(item.fontName=="font_old_10_white_hi.tga"){
      p.setFont(Resources::font());
      clText = clWhite;
      }
    else if(item.fontName=="font_old_20_white.tga"){
      p.setFont(Resources::menuFont());
      } else {
      p.setFont(Resources::menuFont());
      }

    int x = int(w()*item.posx/scriptDiv);
    int y = int(h()*item.posy/scriptDiv);
    int imgX = 0, imgW=0;

    if(hItem.img && !hItem.img->isEmpty()) {
      int32_t dimx = 8192;
      int32_t dimy = 750;

      if(item.dimx!=-1) dimx = item.dimx;
      if(item.dimy!=-1) dimy = item.dimy;

      const int szX = int(w()*dimx/scriptDiv);
      const int szY = int(h()*dimy/scriptDiv);
      p.setBrush(*hItem.img);
      p.drawRect(/*(w()-szX)/2*/x,y,szX,szY,
                 0,0,hItem.img->w(),hItem.img->h());

      imgX = x;
      imgW = szX;
      }

    if(!isEnabled(item))
      clText = clDisabled;

    if(&hItem==selectedItem())
      p.setBrush(clSelected); else
      p.setBrush(clText);

    if(flags & Daedalus::GEngineClasses::C_Menu_Item::IT_TXT_CENTER){
      Size sz = p.font().textSize(textBuf.data());
      if(hItem.img && !hItem.img->isEmpty()) {
        x = imgX+(imgW-sz.w)/2;
        }
      else if(item.dimx!=-1 || item.dimy!=-1) {
        const int szX = int(w()*item.dimx/scriptDiv);
        // const int szY = int(h()*item.dimy/scriptDiv);
        x = x+(szX-sz.w)/2;
        } else {
        x = (w()-sz.w)/2;
        }
      //y += sz.h/2;
      }

    p.drawText(x,y+int(p.font().pixelSize()/2),textBuf.data());
    }

  if(auto sel=selectedItem()){
    p.setFont(Resources::font());
    Daedalus::GEngineClasses::C_Menu_Item& item = sel->get(*vm);
    if(item.text->size()>1) {
      const char* txt = item.text[1].c_str();
      int tw = p.font().textSize(txt).w;

      p.setBrush(clSelected);
      p.drawText((w()-tw)/2,h()-12,txt);
      }
    }
  }

void GameMenu::resizeEvent(SizeEvent &) {
  onTick();
  }

void GameMenu::onMove(int dy) {
  setSelection(int(curItem)+dy, dy>0 ? 1 : -1);
  update();
  }

void GameMenu::onSelect() {
  if(auto sel=selectedItem()){
    Daedalus::GEngineClasses::C_Menu_Item& item = sel->get(*vm);
    exec(item);
    }
  }

void GameMenu::onTick() {
  update();

  const float fx = 640.0f;
  const float fy = 480.0f;

  Daedalus::GEngineClasses::C_Menu& menu = vm->getGameState().getMenu(hMenu);

  if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_DONTSCALE_DIM)
    resize(int(menu.dimx/scriptDiv*fx),int(menu.dimy/scriptDiv*fy));

  if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_ALIGN_CENTER) {
    setPosition((owner.w()-w())/2, (owner.h()-h())/2);
    }
  else if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_DONTSCALE_DIM)
    setPosition(int(menu.posx/scriptDiv*fx), int(menu.posy/scriptDiv*fy));
  }

GameMenu::Item *GameMenu::selectedItem() {
  if(curItem<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS)
    if(hItems[curItem].handle.isValid())
      return &hItems[curItem];
  return nullptr;
  }

void GameMenu::setSelection(int desired,int seek) {
  uint32_t cur=uint32_t(desired);
  for(int i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;++i,cur+=uint32_t(seek)) {
    cur%=Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;

    if(hItems[cur].handle.isValid()) {
      auto& it=hItems[cur].get(*vm);
      if((it.flags&Daedalus::GEngineClasses::C_Menu_Item::EFlags::IT_SELECTABLE) && isEnabled(it)){
        curItem=cur;
        return;
        }
      }
    }
  curItem=uint32_t(-1);
  }

Daedalus::GEngineClasses::C_Menu_Item &GameMenu::Item::get(Daedalus::DaedalusVM &vm) {
  return vm.getGameState().getMenuItem(handle);
  }

void GameMenu::getText(const Daedalus::GEngineClasses::C_Menu_Item& it, std::vector<char> &out) {
  if(out.size()==0)
    out.resize(1);
  out[0]='\0';

  const std::string& src = it.text[0];
  if(it.type==Daedalus::GEngineClasses::C_Menu_Item::MENU_ITEM_TEXT) {
    out.resize(src.size()+1);
    std::memcpy(&out[0],&src[0],src.size());
    out[src.size()] = '\0';
    return;
    }

  if(it.type==Daedalus::GEngineClasses::C_Menu_Item::MENU_ITEM_CHOICEBOX) {
    size_t pos = src.find("#");
    if(pos==std::string::npos)
      pos = src.find("|");
    if(pos==std::string::npos)
      pos = src.size();
    out.resize(pos+1);
    std::memcpy(&out[0],&src[0],pos);
    out[pos] = '\0';
    return;
    }
  }

bool GameMenu::isEnabled(const Daedalus::GEngineClasses::C_Menu_Item &item) {
  if((item.flags & Daedalus::GEngineClasses::C_Menu_Item::IT_ONLY_IN_GAME) && !gothic.isInGame())
    return false;
  if((item.flags & Daedalus::GEngineClasses::C_Menu_Item::IT_ONLY_OUT_GAME) && gothic.isInGame())
    return false;
  return true;
  }

void GameMenu::exec(const Daedalus::GEngineClasses::C_Menu_Item &item) {
  using namespace Daedalus::GEngineClasses::MenuConstants;

  for(auto& str:item.onSelAction_S)
    if(!str.empty())
      if(exec(str))
        return;

  for(auto action:item.onSelAction)
    switch(action) {
      case SEL_ACTION_BACK:
        owner.popMenu();
        return;
      case SEL_ACTION_CLOSE:
        //TODO
        break;
      }
  }

bool GameMenu::exec(const std::string &action) {
  if(action=="NEW_GAME"){
    gothic.onSetWorld(gothic.defaultWorld());
    owner.popMenu();
    return true;
    }

  static const char* menuList[]={
    "MENU_OPTIONS",
    "MENU_SAVEGAME_LOAD",
    "MENU_SAVEGAME_SAVE",
    "MENU_STATUS",
    "MENU_LEAVE_GAME",
    "MENU_OPT_GAME",
    "MENU_OPT_GRAPHICS",
    "MENU_OPT_VIDEO",
    "MENU_OPT_AUDIO",
    "MENU_OPT_CONTROLS",
    "MENU_OPT_CONTROLS_EXTKEYS",
    "MENU_OPT_EXT"
    };

  for(auto subMenu:menuList)
    if(action==subMenu) {
      owner.pushMenu(new GameMenu(owner,gothic,subMenu));
      return false;
      }
  return false;
  }

const char *GameMenu::strEnum(const char *en, int id) {
  int num=0;
  for(size_t i=0;en[i];++i){
    size_t b=i;
    for(size_t r=i;en[r];++r)
      if(en[r]=='|'){
        i=r;
        break;
        }
    if(id==num) {
      size_t sz=i-b;
      textBuf.resize(sz+1);
      std::memcpy(&textBuf[0],&en[b],sz);
      textBuf[sz]='\0';
      return textBuf.data();
      }
    ++num;
    }
  for(size_t i=0;en[i];++i){
    if(en[i]=='#' || en[i]=='|'){
      textBuf.resize(i+1);
      std::memcpy(&textBuf[0],en,i);
      textBuf[i]='\0';
      return textBuf.data();
      }
    }
  return "";
  }

void GameMenu::set(const char *item, const uint32_t value) {
  char buf[16]={};
  std::snprintf(buf,sizeof(buf),"%u",value);
  set(item,buf);
  }

void GameMenu::set(const char *item, const int32_t value) {
  char buf[16]={};
  std::snprintf(buf,sizeof(buf),"%d",value);
  set(item,buf);
  }

void GameMenu::set(const char *item, const int32_t value, const char *post) {
  char buf[32]={};
  std::snprintf(buf,sizeof(buf),"%d%s",value,post);
  set(item,buf);
  }

void GameMenu::set(const char *item, const int32_t value, const int32_t max) {
  char buf[32]={};
  std::snprintf(buf,sizeof(buf),"%d/%d",value,max);
  set(item,buf);
  }

void GameMenu::set(const char *item, const char *value) {
  for(auto& i:hItems)
    if(i.name==item) {
      Daedalus::GEngineClasses::C_Menu_Item& item = i.get(*vm);
      item.text[0]=value;
      return;
      }
  }

void GameMenu::initValues() {
  set("MENU_ITEM_PLAYERGUILD","Debugger");
  set("MENU_ITEM_LEVEL","0");
  }

void GameMenu::setPlayer(const Npc &pl) {
  auto& gilds = gothic.world().getSymbol("TXT_GUILDS");
  auto& tal   = gothic.world().getSymbol("TXT_TALENTS");
  auto& talV  = gothic.world().getSymbol("TXT_TALENTS_SKILLS");

  set("MENU_ITEM_PLAYERGUILD",gilds.getString(pl.guild()).c_str());

  set("MENU_ITEM_LEVEL",      pl.level());
  set("MENU_ITEM_EXP",        pl.experience());
  set("MENU_ITEM_LEVEL_NEXT", pl.experienceNext());
  set("MENU_ITEM_LEARN",      pl.learningPoints());

  set("MENU_ITEM_ATTRIBUTE_1", pl.attribute(Npc::ATR_STRENGTH));
  set("MENU_ITEM_ATTRIBUTE_2", pl.attribute(Npc::ATR_DEXTERITY));
  set("MENU_ITEM_ATTRIBUTE_3", pl.attribute(Npc::ATR_MANA),      pl.attribute(Npc::ATR_MANAMAX));
  set("MENU_ITEM_ATTRIBUTE_4", pl.attribute(Npc::ATR_HITPOINTS), pl.attribute(Npc::ATR_HITPOINTSMAX));

  set("MENU_ITEM_ARMOR_1", pl.protection(Npc::PROT_EDGE));
  set("MENU_ITEM_ARMOR_2", pl.protection(Npc::PROT_POINT)); // not sure about it
  set("MENU_ITEM_ARMOR_3", pl.protection(Npc::PROT_FIRE));
  set("MENU_ITEM_ARMOR_4", pl.protection(Npc::PROT_MAGIC));

  for(size_t i=0;i<Npc::TALENT_MAX;++i){
    auto& str = tal.getString(i);
    if(str.empty())
      continue;

    const int v=pl.talentSkill(Npc::Talent(i));

    char buf[64]={};
    std::snprintf(buf,sizeof(buf),"MENU_ITEM_TALENT_%d_TITLE",i);
    set(buf,str.c_str());

    std::snprintf(buf,sizeof(buf),"MENU_ITEM_TALENT_%d_SKILL",i);
    set(buf,strEnum(talV.getString(i).c_str(),v));

    std::snprintf(buf,sizeof(buf),"MENU_ITEM_TALENT_%d",i);
    set(buf,v,"%");
    }
  }