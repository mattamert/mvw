#pragma once

#include <string>
#include <vector>

class ObjData {
public:
  struct Material {
    float ambient[3];
    float diffuse[3];
    float specular[3];

    float specularExponent;
    float transparency; // 0 is opaque, 1 is fully transparent.
    float indexOfRefraction;

    int illuminationModel; // TODO: Change to enum.

    // TODO: Figure out how to best represent textures...
    //    Shared pointers?
    //    Index into something?
  };

  struct Vertex {
    float pos[3];
    float texCoord[2];
    float normal[3];
  };

  struct Group {
    // TODO: Material.
    std::string name;
    std::vector<uint32_t> indices;
  };

  std::vector<Vertex> m_vertices;
  std::vector<Group> m_groups;

  // TODO: Materials.

  bool ParseObjFile(const std::string& fileName);
};
