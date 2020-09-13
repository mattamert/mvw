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
  Smooth,
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

class Tokenizer {
  const std::vector<char> m_data;
  size_t m_index;

  void ConsumeWhitespace();

 public:
  Tokenizer(std::vector<char>&& file);

  bool AcceptDeclaration(DeclarationType* value);
  bool AcceptInteger(long long* value);
  bool AcceptDouble(double* value);
  bool AcceptIndexSeparator();
  bool AcceptString(std::string* value);
  bool AcceptNewLine();
  void ForceAcceptNewLine();
  bool IsAtEnd();
};

Tokenizer::Tokenizer(std::vector<char>&& file) : m_data(std::move(file)), m_index(0) {}

// NOTE: We don't consider a newline to be whitespace in this scenario, since it is used as a way to
// denote a new statement.
static bool IsWhitespace(char c) {
  return (c == ' ' || c == '\t' || c == '\r' || c == '\v');
}

void Tokenizer::ConsumeWhitespace() {
  while (m_index < m_data.size() && IsWhitespace(m_data[m_index])) {
    m_index++;
  }

  // Parse comments.
  if (m_index < m_data.size() && m_data[m_index] == '#') {
    while (m_index < m_data.size() && m_data[m_index] != '\n') {
      m_index++;
    }
  }
}

static bool ParseDeclarationType(const char* decl, size_t length, DeclarationType* type) {
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
  } else if (length == 1 && strncmp(decl, "s", length) == 0) {
    *type = DeclarationType::Smooth;
  } else if (length == 6 && strncmp(decl, "mtllib", length) == 0) {
    *type = DeclarationType::MTLLib;
  } else if (length == 6 && strncmp(decl, "usemtl", length) == 0) {
    *type = DeclarationType::UseMTL;
  } else {
    return false;
  }

  return true;
}

bool Tokenizer::AcceptDeclaration(DeclarationType* value) {
  ConsumeWhitespace();
  size_t start = m_index;
  size_t end = m_index;
  while (end < m_data.size() && !IsWhitespace(m_data[end]) && m_data[end] != '\n') {
    end++;
  }

  if (ParseDeclarationType(&m_data[start], end - start, value)) {
    m_index = end;
    return true;
  }

  return false;
}

bool Tokenizer::AcceptInteger(long long* value) {
  ConsumeWhitespace();

  const char* startPtr = &m_data[m_index];
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

bool Tokenizer::AcceptDouble(double* value) {
  ConsumeWhitespace();

  const char* startPtr = &m_data[m_index];
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
bool Tokenizer::AcceptIndexSeparator() {
  if (m_index < m_data.size() && m_data[m_index] == '/') {
    m_index++;
    return true;
  }

  return false;
}

bool Tokenizer::AcceptString(std::string* str) {
  ConsumeWhitespace();
  size_t start = m_index;
  size_t end = m_index;
  while (end < m_data.size() && !IsWhitespace(m_data[end]) && m_data[end] != '\n') {
    end++;
  }

  size_t length = end - start;
  if (length > 0) {
    *str = std::string(m_data.data(), start, end - start);
    m_index = end;
    return true;
  }

  return false;
}

bool Tokenizer::AcceptNewLine() {
  ConsumeWhitespace();
  if (m_data[m_index] == '\n') {
    m_index++;
    return true;
  }

  return false;
}

void Tokenizer::ForceAcceptNewLine() {
  while (m_index < m_data.size() && m_data[m_index] != '\n') {
    m_index++;
  }
}

bool Tokenizer::IsAtEnd() {
  ConsumeWhitespace();
  return m_index == m_data.size();
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

  std::unordered_map<Indices, uint32_t, Indices::Hash> m_mapIndicesToVertexIndex;

  void AddGroup(std::string&& groupName);
  bool AddVerticesFromFace(const std::vector<Indices>& face);

 public:
  bool Init(const std::string& filePath);

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
  std::vector<uint32_t> vertexIndices;
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
      vertexIndices.push_back((uint32_t)vertexIndex);
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

bool ObjFileParser::Init(const std::string& filePath) {
  std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
  std::ifstream::pos_type fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> fileContents(fileSize);
  file.read(fileContents.data(), fileSize);
  file.close();

  Tokenizer tokenizer(std::move(fileContents));
  size_t currentLineNumber = 0;
  while (!tokenizer.IsAtEnd()) {
    currentLineNumber++;
    if (tokenizer.AcceptNewLine())
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

        double dummy;
        (void)tokenizer.AcceptDouble(&dummy);  // Third value is allowed, but unused.

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

      case DeclarationType::Smooth: {
        // TODO: Should probably implement this...
        // But for now, let's just igore it becaues ehh.
        std::string dummy;
        (void)tokenizer.AcceptString(&dummy);
      } break;

      case DeclarationType::MTLLib:
        std::cerr << "MTLLib not yet supported." << std::endl;
        break;

      case DeclarationType::UseMTL:
        std::cerr << "UseMTL not yet supported." << std::endl;
        break;

      default:
        parseSucceeded = false;
        break;
    }

    if (parseSucceeded) {
      if (!tokenizer.AcceptNewLine()) {
        tokenizer.ForceAcceptNewLine();
        std::cerr << "WARNING: Additional unparsed information on line " << currentLineNumber
                  << std::endl;
      }
    } else {
      std::cerr << "Parsing failed on line " << currentLineNumber << std::endl;
      return false;
    }
  }

  return true;
}

bool ObjData::ParseObjFile(const std::string& fileName) {
  ObjFileParser parser;
  if (!parser.Init(fileName))
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
