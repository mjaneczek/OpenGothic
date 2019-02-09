#include "world.h"

#include "gothic.h"
#include "resources.h"

#include <zenload/zCMesh.h>
#include <fstream>
#include <functional>
#include <daedalus/DaedalusDialogManager.h>

#include <Tempest/Log>
#include <Tempest/Painter>

World::World(Gothic& gothic,const RendererStorage &storage, std::string file)
  :wname(std::move(file)),gothic(&gothic) {
  using namespace Daedalus::GameState;

  ZenLoad::ZenParser parser(wname,Resources::vdfsIndex());

  // TODO: update loader
  parser.readHeader();

  // TODO: update loader
  ZenLoad::oCWorldData world;
  parser.readWorld(world);

  ZenLoad::PackedMesh mesh;
  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  worldMesh->packMesh(mesh, 1.f, false);

  static_assert(sizeof(Resources::Vertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::Vertex* vert=reinterpret_cast<const Resources::Vertex*>(mesh.vertices.data());
  vbo = Resources::loadVbo<Resources::Vertex>(vert,mesh.vertices.size());

  for(auto& i:mesh.subMeshes){
    blocks.emplace_back();
    Block& b = blocks.back();

    b.ibo     = Resources::loadIbo(i.indices.data(),i.indices.size());
    b.texture = Resources::loadTexture(i.material.texture);
    }

  wdynamic.reset(new DynamicWorld(*this,*worldMesh));

  std::sort(blocks.begin(),blocks.end(),[](const Block& a,const Block& b){ return a.texture<b.texture; });

  if(1){
    for(auto& vob:world.rootVobs)
      loadVob(vob);
    }

  wayNet = std::move(world.waynet);
  for(auto& w:wayNet.waypoints){
    w.position.y = wdynamic->dropRay(w.position.x,w.position.y,w.position.z);
    }

  for(auto& i:wayNet.waypoints)
    if(i.wpName.find("START")==0)
      startPoints.push_back(i);

  wview.reset(new WorldView(*this,storage));

  vm.reset(new WorldScript(*this,gothic,"/_work/data/Scripts/_compiled/GOTHIC.DAT"));
  vm->initDialogs(gothic);
  initScripts(true);

  const char* hero="PC_HERO";
  if(startPoints.size()>0)
    npcPlayer = vm->inserNpc(hero,startPoints[0].wpName.c_str()); else
    npcPlayer = vm->inserNpc(hero,"START");
  }

const ZenLoad::zCWaypointData *World::findPoint(const char *name) const {
  for(auto& i:wayNet.waypoints)
    if(i.wpName==name)
      return &i;
  for(auto& i:freePoints)
    if(i.wpName==name)
      return &i;
  for(auto& i:startPoints)
    if(i.wpName==name)
      return &i;
  return nullptr;
  }

StaticObjects::Mesh World::getView(const std::string &visual) {
  return getView(visual,0,0,0);
  }

StaticObjects::Mesh World::getView(const std::string &visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) {
  return view()->getView(visual,headTex,teetTex,bodyColor);
  }

DynamicWorld::Item World::getPhysic() {
  return physic()->ghostObj();
  }

void World::updateAnimation() {
  if(!vm)
    return;
  auto& w=*vm;
  for(size_t i=0;i<w.npcCount();++i)
    w.npc(i).updateAnimation();
  }

void World::tick(uint64_t dt) {
  if(!vm)
    return;
  vm->tick(dt);
  }

uint64_t World::tickCount() const {
  return vm ? vm->tickCount() : 0;
  }

Daedalus::PARSymbol &World::getSymbol(const char *s) const {
  return vm->getSymbol(s);
  }

Interactive *World::findFocus(const Npc &pl, const Tempest::Matrix4x4 &v, int w, int h) {
  Interactive* ret=nullptr;
  float rlen = w*h;
  if(view()==nullptr)
    return nullptr;

  auto mvp = view()->viewProj(v);

  for(auto& i:interactiveObj){
    auto m = mvp;
    m.mul(i.objMat);

    auto pos = i.position();
    float dx=pl.position()[0]-pos[0];
    float dy=pl.position()[1]-pos[1];
    float dz=pl.position()[2]-pos[2];

    float l = std::sqrt(dx*dx+dy*dy+dz*dz);
    if(l>250)
      continue;

    float x=0,y=0,z=0;
    m.project(x,y,z);

    if(z<0.f || z>1.f)
      continue;

    x = 0.5f*x*w;
    y = 0.5f*y*h;

    l = std::sqrt(x*x+y*y);
    if(l<rlen){
      rlen=l;
      ret=&i;
      }
    }

  return ret;
  }

Interactive *World::findFocus(const Tempest::Matrix4x4 &mvp, int w, int h) {
  if(npcPlayer==nullptr)
    return nullptr;
  return findFocus(*npcPlayer,mvp,w,h);
  }

void World::marchInteractives(Tempest::Painter &p,const Tempest::Matrix4x4& mvp,
                              int w,int h) const {
  for(auto& i:interactiveObj){
    auto m = mvp;
    m.mul(i.objMat);

    float x=0,y=0,z=0;
    m.project(x,y,z);

    if(z<0.f || z>1.f)
      continue;

    x = (0.5f*x+0.5f)*w;
    y = (0.5f*y+0.5f)*h;

    p.setBrush(Tempest::Color(1,1,1,1));
    p.drawRect(int(x),int(y),1,1);

    i.marchInteractives(p,mvp,w,h);
    }
  }

int32_t World::runFunction(const std::string& fname, bool clearDataStack) {
  return vm->runFunction(fname,clearDataStack);
  }

void World::initScripts(bool firstTime) {
  auto dot=wname.rfind('.');
  std::string startup = (firstTime ? "startup_" : "init_")+(dot==std::string::npos ? wname : wname.substr(0,dot));

  if(vm->hasSymbolName(startup))
    vm->runFunction(startup,true);
  }

void World::loadVob(const ZenLoad::zCVobData &vob) {
  for(auto& i:vob.childVobs)
    loadVob(i);

  if(vob.objectClass=="zCVob" ||
     vob.objectClass=="oCMOB:zCVob" ||
     vob.objectClass=="zCPFXControler:zCVob" ||
     vob.objectClass=="oCMobFire:oCMobInter:oCMOB:zCVob") {
    addStatic(vob);
    }
  else if(vob.objectClass=="oCMobInter:oCMOB:zCVob" ||
          vob.objectClass=="oCMobContainer:oCMobInter:oCMOB:zCVob" ||
          vob.objectClass=="oCMobDoor:oCMobInter:oCMOB:zCVob" ||
          vob.objectClass=="oCMobSwitch:oCMobInter:oCMOB:zCVob"){
    addInteractive(vob);
    }
  else if(vob.objectClass=="zCVobAnimate:zCVob"){ // ork flags
    addStatic(vob); //TODO: morph animation
    }
  else if(vob.objectClass=="zCVobLevelCompo:zCVob"){
    return;
    }
  else if(vob.objectClass=="zCMover:zCTrigger:zCVob"){
    addStatic(vob); // castle gate
    }
  else if(vob.objectClass=="oCTriggerScript:zCTrigger:zCVob" ||
          vob.objectClass=="oCTriggerChangeLevel:zCTrigger:zCVob" ||
          vob.objectClass=="zCTrigger:zCVob"){
    }
  else if(vob.objectClass=="zCZoneZFog:zCVob" ||
          vob.objectClass=="zCZoneZFogDefault:zCZoneZFog:zCVob"){
    }
  else if(vob.objectClass=="zCZoneVobFarPlaneDefault:zCZoneVobFarPlane:zCVob" ||
          vob.objectClass=="zCVobLensFlare:zCVob"){
    return;
    }
  else if(vob.objectClass=="zCVobStartpoint:zCVob") {
    ZenLoad::zCWaypointData point={};
    point.wpName   = vob.vobName;
    point.position = vob.position;
    startPoints.push_back(point);
    }
  else if(vob.objectClass=="zCVobSpot:zCVob") {
    ZenLoad::zCWaypointData point={};
    point.wpName   = vob.vobName;
    point.position = vob.position;
    point.position.y = wdynamic->dropRay(vob.position.x,vob.position.y,vob.position.z);
    freePoints.push_back(point);
    }
  else if(vob.objectClass=="oCItem:zCVob") {
    }
  else if(vob.objectClass=="zCVobSound" ||
          vob.objectClass=="zCVobSound:zCVob" ||
          vob.objectClass=="zCVobSoundDaytime:zCVobSound:zCVob") {
    }
  else if(vob.objectClass=="oCZoneMusic:zCVob" ||
          vob.objectClass=="oCZoneMusicDefault:oCZoneMusic:zCVob") {
    }
  else if(vob.objectClass=="zCVobLight:zCVob") {
    }
  else {
    static std::unordered_set<std::string> cls;
    if(cls.find(vob.objectClass)==cls.end()){
      cls.insert(vob.objectClass);
      Tempest::Log::d("unknown vob class ",vob.objectClass);
      }
    }
  }

void World::addStatic(const ZenLoad::zCVobData &vob) {
  Dodad d;
  d.mesh = Resources::loadMesh(vob.visual);
  if(d.mesh) {
    float v[16]={};
    std::memcpy(v,vob.worldMatrix.m,sizeof(v));
    d.objMat = Tempest::Matrix4x4(v);

    staticObj.emplace_back(d);
    }
  }

void World::addInteractive(const ZenLoad::zCVobData &vob) {
  Interactive d(*this,vob);

  if(d.mesh)
    interactiveObj.emplace_back(std::move(d));
  }