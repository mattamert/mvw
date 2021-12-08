#pragma once

#include <filesystem>
#include <string>
#include <vector>

class ObjFileData {
 public:
  // TODO: Currently, none of the texture maps' options are handled. If we wish to fullky support
  // the obj file format, these should be added in.

  //enum IMFChannel {
  //  Red,
  //  Green,
  //  Blue,
  //  Matte,
  //  Luminance,
  //  ZDepth,
  //};

  //struct Texture {
  //  std::filesystem::path file;
  //  bool blendU = true;
  //  bool blendV = true;
  //  float bumpMultiplier = 1.f;
  //  float boostValue = 0.f; // Boosts sharpness.
  //  bool colorCorrection = false;
  //  bool clamp = false;
  //  IMFChannel imfChannel = IMFChannel::Luminance;
  //  float base = 0.f;
  //  float gain = 1.f;
  //  float textureMapOrigin[3] = { 0.f, 0.f, 0.f };
  //  float textureSize[3] = { 1.f, 1.f, 1.f };
  //  float turbulance[3] = { 0.f, 0.f, 0.f };
  //  float textureRes;
  //};

  struct Texture {
    std::filesystem::path file;
  };

  //enum IlluminationModel {
  //  ColorOnAndAmbientOff = 0,
  //  ColoronandAmbienton = 1,
  //  HighlightOn = 2,
  //  ReflectionOnAndRayTraceOn = 3,
  //  TransparencyGlassOn = 4,
  //  ReflectionRayTraceOn = 5,
  //  ReflectionFresnelOnAndRayTraceOn = 6,
  //  TransparencyRefractionOn = 7,
  //  ReflectionFresnelOffAndRayTraceOn = 8,
  //  TransparencyRefractionOn = 9,
  //  ReflectionFresnelOnAndRayTraceOn = 10,
  //  ReflectionOnAndRayTraceOff = 11,
  //  TransparencyGlassOn = 12,
  //  ReflectionRayTraceoff = 13,
  //  CastsShadowsOntoInvisibleSurfaces = 14,
  //};

  struct Color {
    float r;
    float g;
    float b;
  };

  // The .mtl file format supports lots and lots of options for materials. For now, let's only
  // support a subset.
  struct Material {
    std::string name;

    Color ambientColor = {0.f, 0.f, 0.f};
    Color diffuseColor = {0.f, 0.f, 0.f};
    Color specularColor = {0.f, 0.f, 0.f};
    float specularExponent = 1.f;

    // The sharpness of the local reflection map. Value between 0 and 1000.
    //float sharpness = 60.f;
    //float transmissionFilterColor[3] = { 1.f, 1.f, 1.f };

    // Value between 0.001 and 10; 1.0 means that light does not bend when passing through.
    //float indexOfRefraction = 1.f;
    //bool antiAliasTextures = true;
    //IlluminationModel illuminationModel;

    Texture diffuseMap;
    //Texture ambientMap;
    //Texture specularMap;
    //Texture specularExponentMap;
    //Texture dissolveMap;
    //Texture displacementMap;
    //Texture decalMap;
    //Texture bumpMap;
    //Texture reflectionMap;
  };

  struct Vertex {
    float pos[3];
    float texCoord[2];
    float normal[3];
  };

  struct MeshPart {
    uint32_t indexStart;
    uint32_t numIndices;
    uint32_t materialIndex;
  };

  struct AxisAlignedBounds {
    float max[3];
    float min[3];
  };

  std::vector<Vertex> m_vertices;
  std::vector<uint32_t> m_indices;
  std::vector<MeshPart> m_meshParts;
  std::vector<Material> m_materials;
  AxisAlignedBounds m_bounds;

  bool ParseObjFile(const std::string& fileName);
};
