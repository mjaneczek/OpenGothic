#pragma once

#include <Tempest/Assets>
#include <Tempest/Font>
#include <Tempest/Texture2d>
#include <Tempest/Device>
#include <Tempest/SoundDevice>

#include <vdfs/fileIndex.h>

#include <zenload/zCModelMeshLib.h>
#include <zenload/zTypes.h>

#include <tuple>

#include "graphics/material.h"
#include "world/soundfx.h"

class Gothic;
class StaticMesh;
class ProtoMesh;
class Skeleton;
class Animation;
class AttachBinder;
class PhysicMeshShape;
class PfxEmitterMesh;
class SoundFx;
class GthFont;

namespace Dx8 {
class DirectMusic;
class PatternList;
}

class Resources final {
  public:
    explicit Resources(Gothic& gothic, Tempest::Device& device);
    ~Resources();

    enum class FontType : uint8_t {
      Normal,
      Hi,
      Disabled,
      Yellow,
      Red
      };

    enum {
      MaxFramesInFlight = 2,
      ShadowLayers      = 2
      };

    static const size_t MAX_NUM_SKELETAL_NODES = 96;

    struct Vertex {
      float    pos[3];
      float    norm[3];
      float    uv[2];
      uint32_t color;
      };

    struct VertexA {
      float    norm[3];
      float    uv[2];
      uint32_t color/*unused*/;
      float    pos[4][3];
      uint8_t  boneId[4];
      float    weights[4];
      };

    struct VertexFsq {
      float    pos[2];
      };

    struct VertexL {
      Tempest::Vec3 pos;
      Tempest::Vec4 cen;
      Tempest::Vec3 color;
      };

    static const char* renderer();
    static void        waitDeviceIdle();

    static const Tempest::Sampler2d& shadowSampler();

    static const GthFont& dialogFont();
    static const GthFont& font();
    static const GthFont& warningFont();
    static const GthFont& font(FontType type);
    static const GthFont& font(const char *fname,FontType type = FontType::Normal);

    static const Tempest::Texture2d& fallbackTexture();
    static const Tempest::Texture2d& fallbackBlack();
    static const Tempest::Texture2d* loadTexture(const char* name);
    static const Tempest::Texture2d* loadTexture(const std::string& name);
    static const Tempest::Texture2d* loadTexture(const std::string& name,int32_t v,int32_t c);
    static auto                      loadTextureAnim(const std::string& name) -> std::vector<const Tempest::Texture2d*>;
    static       Tempest::Texture2d  loadTexture(const Tempest::Pixmap& pm);
    static       Material            loadMaterial(const ZenLoad::zCMaterialData& src, bool enableAlphaTest);

    static const AttachBinder*       bindMesh      (const ProtoMesh& anim,const Skeleton& s);
    static const ProtoMesh*          loadMesh      (const std::string& name);
    static const PfxEmitterMesh*     loadEmiterMesh(const char*        name);
    static const Skeleton*           loadSkeleton  (const char*        name);
    static const Animation*          loadAnimation (const std::string& name);

    static Tempest::SoundEffect*     loadSound(const char* name);
    static Tempest::SoundEffect*     loadSound(const std::string& name);

    static Tempest::Sound            loadSoundBuffer(const std::string& name);
    static Tempest::Sound            loadSoundBuffer(const char*        name);

    static Dx8::PatternList          loadDxMusic(const char *name);
    static const ProtoMesh*          decalMesh(const ZenLoad::zCVobData& vob);

    template<class V>
    static Tempest::VertexBuffer<V>  vbo(const V* data,size_t sz){ return inst->device.vbo(data,sz); }

    template<class V>
    static Tempest::IndexBuffer<V>   ibo(const V* data,size_t sz){ return inst->device.ibo(data,sz); }

    static std::vector<uint8_t>      getFileData(const char*        name);
    static bool                      getFileData(const char*        name,std::vector<uint8_t>& dat);
    static std::vector<uint8_t>      getFileData(const std::string& name);

    static bool                      hasFile(const std::string& fname);
    static VDFS::FileIndex&          vdfsIndex();

    static const Tempest::VertexBuffer<VertexFsq>& fsqVbo();

  private:
    static Resources* inst;

    enum class MeshLoadCode : uint8_t {
      Error,
      Static,
      Dynamic
      };

    struct Archive {
      std::u16string name;
      int64_t        time=0;
      uint16_t       ord=0;
      bool           isMod=false;
      };

    struct DecalK {
      Material mat;
      float    sX = 1;
      float    sY = 1;
      bool     decal2Sided = false;
      bool     operator == (const DecalK& other) const {
        return mat        ==other.mat &&
               sX         ==other.sX &&
               sY         ==other.sY &&
               decal2Sided==other.decal2Sided;
        }
      };

    using TextureCache = std::unordered_map<std::string,std::unique_ptr<Tempest::Texture2d>>;

    int64_t               vdfTimestamp(const std::u16string& name);
    void                  detectVdf(std::vector<Archive>& ret, const std::u16string& root);

    Tempest::Texture2d*   implLoadTexture(TextureCache& cache, const char* cname);
    Tempest::Texture2d*   implLoadTexture(TextureCache& cache, std::string &&name, const std::vector<uint8_t> &data);
    ProtoMesh*            implLoadMesh(const std::string &name);
    ProtoMesh*            implDecalMesh(const ZenLoad::zCVobData& vob);
    Skeleton*             implLoadSkeleton(std::string name);
    Animation*            implLoadAnimation(std::string name);
    Tempest::Sound        implLoadSoundBuffer(const char* name);
    Tempest::SoundEffect* implLoadSound(const char *name);
    Dx8::PatternList      implLoadDxMusic(const char *name);
    GthFont&              implLoadFont(const char* fname, FontType type);
    PfxEmitterMesh*       implLoadEmiterMesh(const char* name);

    MeshLoadCode          loadMesh(ZenLoad::PackedMesh &sPacked, ZenLoad::zCModelMeshLib &lib, std::string  name);
    ZenLoad::zCModelMeshLib loadMDS (std::string& name);
    Tempest::VertexBuffer<Vertex> sphere(int passCount, float R);

    Tempest::Texture2d fallback, fbZero;

    using BindK  = std::tuple<const Skeleton*,const ProtoMesh*>;
    using FontK  = std::pair<const std::string,FontType>;

    struct Hash {
      size_t operator()(const BindK& b) const {
        return std::uintptr_t(std::get<0>(b));
        }
      size_t operator()(const DecalK& b) const {
        return std::uintptr_t(b.mat.tex);
        }
      size_t operator()(const FontK& b) const {
        std::hash<std::string> h;
        return h(std::get<0>(b));
        }
      };

    Tempest::Device&      device;
    Tempest::SoundDevice  sound;
    std::recursive_mutex  sync;
    std::unique_ptr<Dx8::DirectMusic> dxMusic;
    Gothic&               gothic;
    VDFS::FileIndex       gothicAssets;

    std::vector<uint8_t>  fBuff, ddsBuf;
    Tempest::VertexBuffer<VertexFsq>         fsq;

    TextureCache                                                          texCache;

    std::unordered_map<std::string,std::unique_ptr<ProtoMesh>>            aniMeshCache;
    std::unordered_map<DecalK,std::unique_ptr<ProtoMesh>,Hash>            decalMeshCache;
    std::unordered_map<std::string,std::unique_ptr<Skeleton>>             skeletonCache;
    std::unordered_map<std::string,std::unique_ptr<Animation>>            animCache;
    std::unordered_map<BindK,std::unique_ptr<AttachBinder>,Hash>          bindCache;
    std::unordered_map<std::string,std::unique_ptr<PfxEmitterMesh>>       emiMeshCache;

    std::unordered_map<std::string,std::unique_ptr<Tempest::SoundEffect>> sndCache;
    std::unordered_map<FontK,std::unique_ptr<GthFont>,Hash>               gothicFnt;
  };


namespace Tempest {

template<>
inline VertexBufferDecl vertexBufferDecl<Resources::Vertex>() {
  return {Decl::float3,Decl::float3,Decl::float2,Decl::color};
  }

template<>
inline VertexBufferDecl vertexBufferDecl<Resources::VertexA>() {
  return {Decl::float3,Decl::float2,Decl::color,
          Decl::float3,Decl::float3,Decl::float3,Decl::float3,
          Decl::color,Decl::float4};
  }

template<>
inline VertexBufferDecl vertexBufferDecl<Resources::VertexFsq>() {
  return {Decl::float2};
  }

template<>
inline VertexBufferDecl vertexBufferDecl<Resources::VertexL>() {
  return {Decl::float3,Decl::float4,Decl::float3};
  }

}
