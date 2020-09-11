#include "ObjFileLoader.h"

// Temporary.
#include <iostream>

#include <assert.h>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>

enum class DeclarationType {
  Position,
  TextureCoord,
  Normal,
  Face,
  Group,
  MTLLib,
  UseMTL,
};

struct Position {
  // 4d coordinates are converted to 3d coords.
  double x;
  double y;
  double z;
};

struct TexCoord {
  // 3d textures are not supported right now.
  double u;
  double v;
};

struct Normal {
  double x;
  double y;
  double z;
};

// These indices are 1-based, as that is what is given by the obj file.
struct Indices {
  unsigned long long posIndex;

  // If texCoordIndex or normalIndex are not specified, they will be set to 0.
  unsigned long long texCoordIndex;
  unsigned long long normalIndex;

  struct Hash {
    size_t operator()(const Indices& indices) const;
  };

  bool operator==(const Indices& other) const {
    return (this->posIndex == other.posIndex && this->texCoordIndex == other.texCoordIndex &&
            this->normalIndex == other.normalIndex);
  }
};

template <class T>
void hash_combine(std::size_t& s, const T& v);

size_t Indices::Hash::operator()(const Indices& indices) const {
  size_t hash = 0;
  hash_combine(hash, indices.posIndex);
  hash_combine(hash, indices.texCoordIndex);
  hash_combine(hash, indices.normalIndex);
  return hash;
}

class ObjLineTokenizer {
  std::string m_line;
  size_t m_index;

  void ConsumeWhitespace();

 public:
  ObjLineTokenizer(std::string&& line);

  bool AcceptDeclaration(DeclarationType* value);
  bool AcceptInteger(long long* value);
  bool AcceptDouble(double* value);
  bool AcceptIndexSeparator();
  bool AcceptString(std::string* value);
  bool IsAtEnd();
};

ObjLineTokenizer::ObjLineTokenizer(std::string&& line) : m_line(std::move(line)), m_index(0) {}

static bool IsWhitespace(char c) {
  return (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v');
}

void ObjLineTokenizer::ConsumeWhitespace() {
  while (m_index < m_line.size() && IsWhitespace(m_line[m_index])) {
    m_index++;
  }

  // Parse comments.
  if (m_index < m_line.size() && m_line[m_index] == '#')
    m_index = m_line.size();
}

static bool ParseDeclarationType(char* decl, size_t length, DeclarationType* type) {
  // strncmp doesn't quite work how I thought it should, so we need to compare the length as well.
  if (length == 2 && strncmp(decl, "vt", length) == 0) {
    *type = DeclarationType::TextureCoord;
  } else if (length == 2 && strncmp(decl, "vn", length) == 0) {
    *type = DeclarationType::Normal;
  } else if (length == 1 && strncmp(decl, "v", length) == 0) {
    *type = DeclarationType::Position;
  } else if (length == 1 && strncmp(decl, "f", length) == 0) {
    *type = DeclarationType::Face;
  } else if (length == 1 && strncmp(decl, "g", length) == 0) {
    *type = DeclarationType::Group;
  } else if (length == 6 && strncmp(decl, "mtllib", length) == 0) {
    *type = DeclarationType::MTLLib;
  } else if (length == 6 && strncmp(decl, "usemtl", length) == 0) {
    *type = DeclarationType::UseMTL;
  } else {
    return false;
  }

  return true;
}

bool ObjLineTokenizer::AcceptDeclaration(DeclarationType* value) {
  ConsumeWhitespace();
  size_t start = m_index;
  size_t end = m_index;
  while (end < m_line.size() && !IsWhitespace(m_line[end])) {
    end++;
  }

  if (ParseDeclarationType(&m_line[start], end - start, value)) {
    m_index = end;
    return true;
  }

  return false;
}

bool ObjLineTokenizer::AcceptInteger(long long* value) {
  ConsumeWhitespace();

  const char* startPtr = &m_line[m_index];
  char* endPtr;
  long long parsedValue = strtoll(startPtr, &endPtr, /*radix*/ 10);

  size_t lengthConsumed = endPtr - startPtr;
  if (lengthConsumed > 0) {
    m_index += lengthConsumed;
    *value = parsedValue;
    return true;
  }

  return false;
}

bool ObjLineTokenizer::AcceptDouble(double* value) {
  ConsumeWhitespace();

  const char* startPtr = &m_line[m_index];
  char* endPtr;
  double parsedValue = strtod(startPtr, &endPtr);

  size_t lengthConsumed = endPtr - startPtr;
  if (lengthConsumed > 0) {
    m_index += lengthConsumed;
    *value = parsedValue;
    return true;
  }

  return false;
}
bool ObjLineTokenizer::AcceptIndexSeparator() {
  if (m_index < m_line.size() && m_line[m_index] == '/') {
    m_index++;
    return true;
  }

  return false;
}

bool ObjLineTokenizer::AcceptString(std::string* str) {
  ConsumeWhitespace();
  size_t start = m_index;
  size_t end = m_index;
  while (end < m_line.size() && !IsWhitespace(m_line[end])) {
    end++;
  }

  size_t length = end - start;
  if (length > 0) {
    *str = std::string(m_line, start, end - start);
    m_index = end;
    return true;
  }

  return false;
}

bool ObjLineTokenizer::IsAtEnd() {
  ConsumeWhitespace();
  return m_index == m_line.size();
}

class ObjFileParser {
  // Overall result.
  std::vector<ObjData::Vertex> m_vertices;
  std::vector<ObjData::Group> m_groups;

  // Only used for parsing.
  ObjData::Group* m_currentGroup = nullptr;
  std::vector<Position> m_positions;
  std::vector<TexCoord> m_texCoords;
  std::vector<Normal> m_normals;

  std::unordered_map<Indices, size_t, Indices::Hash> m_mapIndicesToVertexIndex;

  void AddGroup(std::string&& groupName);
  bool AddVerticesFromFace(const std::vector<Indices>& face);
  // bool HandleFace(const std::vector<Indices>& face);

 public:
  bool Init(std::istream& input);

  std::vector<ObjData::Vertex>& GetVertices() { return m_vertices; }
  std::vector<ObjData::Group>& GetGroups() { return m_groups; }
};

void ObjFileParser::AddGroup(std::string&& groupName) {
  m_groups.emplace_back();
  m_groups.back().name = std::move(groupName);
  m_currentGroup = &m_groups.back();
}

bool ObjFileParser::AddVerticesFromFace(const std::vector<Indices>& face) {
  // TODO: Generate normals if needed.
  std::vector<size_t> vertexIndices;
  for (const Indices indices : face) {
    assert(indices.posIndex > 0);
    // TODO: We should be able to handle the case where we don't have texture coordinates.
    // Just use (0, 0).
    assert(indices.texCoordIndex > 0);
    assert(indices.normalIndex > 0);

    auto iter = m_mapIndicesToVertexIndex.find(indices);
    if (iter == m_mapIndicesToVertexIndex.end()) {
      const Position& pos = m_positions[indices.posIndex - 1];
      const TexCoord& texCoord = m_texCoords[indices.texCoordIndex - 1];
      const Normal& normal = m_normals[indices.normalIndex - 1];

      ObjData::Vertex vertex;
      vertex.pos[0] = pos.x;
      vertex.pos[1] = pos.y;
      vertex.pos[2] = pos.z;
      vertex.texCoord[0] = texCoord.u;
      vertex.texCoord[1] = texCoord.v;
      vertex.normal[0] = normal.x;
      vertex.normal[1] = normal.y;
      vertex.normal[2] = normal.z;
      m_vertices.emplace_back(vertex);

      size_t vertexIndex = m_vertices.size() - 1;
      m_mapIndicesToVertexIndex[indices] = vertexIndex;
      vertexIndices.push_back(vertexIndex);
    } else {
      vertexIndices.push_back(iter->second);
    }
  }

  if (!m_currentGroup)
    AddGroup("");

  assert(vertexIndices.size() >= 3);
  assert(m_currentGroup != nullptr);

  size_t base = vertexIndices[0];
  size_t prev = vertexIndices[1];
  for (size_t i = 2; i < vertexIndices.size(); ++i) {
    size_t curr = vertexIndices[i];
    m_currentGroup->indices.push_back(base);
    m_currentGroup->indices.push_back(prev);
    m_currentGroup->indices.push_back(curr);
    prev = curr;
  }

  return true;
}

static bool ResolveRelativeIndex(long long index, size_t referenceSize, unsigned long long* resolvedIndex) {
  if (index < 0) {
    if (referenceSize < -index) {
      return false;
    }
    *resolvedIndex = (long long)referenceSize + index + 1;
  } else {
    *resolvedIndex = (unsigned long long)index;
  }

  return true;
}

bool ObjFileParser::Init(std::istream& input) {
  size_t currentLineNumber = 0;
  std::string line;
  while (std::getline(input, line)) {
    currentLineNumber++;

    ObjLineTokenizer tokenizer(std::move(line));
    if (tokenizer.IsAtEnd())
      continue;

    DeclarationType type;
    bool parseSucceeded = tokenizer.AcceptDeclaration(&type);
    if (!parseSucceeded) {
      std::cerr << "Unrecognized declaration" << std::endl;
      continue;
    }

    switch (type) {
      case DeclarationType::Position: {
        Position pos = {};
        parseSucceeded &= tokenizer.AcceptDouble(&pos.x);
        parseSucceeded &= tokenizer.AcceptDouble(&pos.y);
        parseSucceeded &= tokenizer.AcceptDouble(&pos.z);
        // Always append a position even if parsing failed, so that future indices do not get off.
        m_positions.push_back(pos);
      } break;

      case DeclarationType::TextureCoord: {
        TexCoord coord = {};
        parseSucceeded &= tokenizer.AcceptDouble(&coord.u);
        if (!tokenizer.AcceptDouble(&coord.v))  // Second value is optional.
          coord.v = 0.;

        // Always append a texcoord even if parsing failed, so that future indices do not get off.
        m_texCoords.push_back(coord);
      } break;

      case DeclarationType::Normal: {
        Normal normal = {};
        parseSucceeded &= tokenizer.AcceptDouble(&normal.x);
        parseSucceeded &= tokenizer.AcceptDouble(&normal.y);
        parseSucceeded &= tokenizer.AcceptDouble(&normal.z);
        // Always append a normal even if parsing failed, so that future indices do not get off.
        m_normals.push_back(normal);
      } break;

      case DeclarationType::Face: {
        std::vector<Indices> indices;
        for (;;) {
          long long posIndex = 0;
          long long texCoordIndex = 0;
          long long normalIndex = 0;

          if (!tokenizer.AcceptInteger(&posIndex))
            break;

          if (tokenizer.AcceptIndexSeparator()) {
            (void)tokenizer.AcceptInteger(&texCoordIndex);  // Can be unspecified.
            if (tokenizer.AcceptIndexSeparator()) {
              parseSucceeded &= tokenizer.AcceptInteger(&normalIndex);
            }
          }

          Indices curr;
          parseSucceeded &= ResolveRelativeIndex(posIndex, m_positions.size(), &curr.posIndex);
          parseSucceeded &=
              ResolveRelativeIndex(texCoordIndex, m_texCoords.size(), &curr.texCoordIndex);
          parseSucceeded &= ResolveRelativeIndex(normalIndex, m_normals.size(), &curr.normalIndex);

          indices.emplace_back(curr);
        }

        if (indices.size() < 3) {
          parseSucceeded = false;
        } else if (parseSucceeded) {
          parseSucceeded &= AddVerticesFromFace(indices);
        }
      } break;

      case DeclarationType::Group: {
        std::vector<std::string> groupNames;
        std::string currentName;
        while (tokenizer.AcceptString(&currentName)) {
          groupNames.emplace_back(std::move(currentName));
        }

        // TOOD: Support multiple group names on one line.
        if (groupNames.size() > 1) {
          std::cerr << "WARNING: This only supports one group name per group definition."
                    << std::endl;
        }

        if (groupNames.empty()) {
          AddGroup("");
        } else {
          AddGroup(std::move(groupNames[0]));
        }
      } break;

      case DeclarationType::MTLLib:
        std::cerr << "MTLLib not yet supported..." << std::endl;
        break;

      case DeclarationType::UseMTL:
        std::cerr << "UseMTL not yet supported..." << std::endl;
        break;

      default:
        parseSucceeded = false;
        break;
    }

    if (parseSucceeded && !tokenizer.IsAtEnd()) {
      std::cerr << "WARNING: Additional unparsed information on line " << currentLineNumber << std::endl;
    } else if (!parseSucceeded) {
      std::cerr << "Parsing failed on line " << currentLineNumber << std::endl;
      return false;
    }
  }

  return true;
}

bool ObjData::ParseObjFile(const std::string& fileName) {
  std::ifstream file(fileName);
  ObjFileParser parser;
  if (!parser.Init(file))
    return false;

  m_vertices = std::move(parser.GetVertices());
  m_groups = std::move(parser.GetGroups());
  return true;
}

// The source of hash_combine was taken from the boost library. Boost's license is as follow:
/*
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license(the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third - parties to whom the Software is furnished to
do so, all subject to the following :

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine - executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON - INFRINGEMENT.IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/
template <class T>
inline void hash_combine(std::size_t& s, const T& v) {
  std::hash<T> h;
  s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
}
