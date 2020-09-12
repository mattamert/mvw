#pragma once

#include <string>
#include <vector>

class ObjData {
public:
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
