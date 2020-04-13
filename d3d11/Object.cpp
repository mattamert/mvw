#include "Object.h"

#include <assert.h>
#include <vector>

DirectX::XMFLOAT4X4 Object::GenerateWorldTransform() {
  DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(scale_, scale_, scale_);
  DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationY(rotation_y_);
  DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(position_.x, position_.y, position_.z);

  DirectX::XMMATRIX world_transform = scale * rotation * translation;
  DirectX::XMFLOAT4X4 result;
  DirectX::XMStoreFloat4x4(&result, world_transform);
  return result;
}

// This is for just a square. Currently just keeping for funsies.
  //VertexData vertices[NUM_VERTICES] = {
  //  { -0.5f, -0.5f, 0.f,    1.f, 0.f, 0.f },
  //  { -0.5f,  0.5f, 0.f,    0.f, 1.f, 0.f },
  //  {  0.5f,  0.5f, 0.f/*4.f*/,    0.f, 0.f, 1.f },

  //  //{ -0.5f, -0.5f, 0.f,    1.f, 0.f, 0.f },
  //  //{  0.5f,  0.5f, 0.f/*4.f*/,    0.f, 0.f, 1.f },
  //  {  0.5f, -0.5f, 0.f/*4.f*/,    0.f, 1.f, 0.f },
  //  //{ 0.0f, 0.5f, 0.0f,    1.f, 0.f, 0.f },
  //  //{ 0.45f, -0.5, 0.0f,    0.f, 1.f, 0.f },
  //  //{-0.45f, -0.5f, 0.0f,    0.f, 0.f, 1.f },
  //};
  //uint32_t indicies[] = { 0, 1, 2, 0, 2, 3 };

HRESULT CubeObject::Initialize(ID3D11Device1* device) {
  ShadedVertex vertices[] =
  {
  // +x direction
  { +1.0f, -1.0f, -1.0f,    1.f, 0.f, 0.f },
  { +1.0f, +1.0f, -1.0f,    1.f, 0.f, 0.f },
  { +1.0f, +1.0f, +1.0f,    1.f, 0.f, 0.f },
  { +1.0f, -1.0f, +1.0f,    1.f, 0.f, 0.f },

  // -x direction
  { -1.0f, -1.0f, +1.0f,    -1.f, 0.f, 0.f },
  { -1.0f, +1.0f, +1.0f,    -1.f, 0.f, 0.f },
  { -1.0f, +1.0f, -1.0f,    -1.f, 0.f, 0.f },
  { -1.0f, -1.0f, -1.0f,    -1.f, 0.f, 0.f },

  // +y direction
  { +1.0f, +1.0f, +1.0f,    0.f, 1.f, 0.f },
  { +1.0f, +1.0f, -1.0f,    0.f, 1.f, 0.f },
  { -1.0f, +1.0f, -1.0f,    0.f, 1.f, 0.f },
  { -1.0f, +1.0f, +1.0f,    0.f, 1.f, 0.f },

  // -y direction
  { +1.0f, -1.0f, +1.0f,    0.f, -1.f, 0.f },
  { +1.0f, -1.0f, -1.0f,    0.f, -1.f, 0.f },
  { -1.0f, -1.0f, -1.0f,    0.f, -1.f, 0.f },
  { -1.0f, -1.0f, +1.0f,    0.f, -1.f, 0.f },

  // +z direction
  { +1.0f, -1.0f, +1.0f,    0.f, 0.f, 1.f },
  { +1.0f, +1.0f, +1.0f,    0.f, 0.f, 1.f },
  { -1.0f, +1.0f, +1.0f,    0.f, 0.f, 1.f },
  { -1.0f, -1.0f, +1.0f,    0.f, 0.f, 1.f },

  // -z direction
  { -1.0f, -1.0f, -1.0f,    0.f, 0.f, -1.f },
  { -1.0f, +1.0f, -1.0f,    0.f, 0.f, -1.f },
  { +1.0f, +1.0f, -1.0f,    0.f, 0.f, -1.f },
  { +1.0f, -1.0f, -1.0f,    0.f, 0.f, -1.f },
  };

  std::vector<uint32_t> indices;
  for (size_t i = 0; i < 6; ++i) {
    uint32_t starting_index = i * 4;
    indices.push_back(starting_index);
    indices.push_back(starting_index + 1);
    indices.push_back(starting_index + 2);

    indices.push_back(starting_index);
    indices.push_back(starting_index + 2);
    indices.push_back(starting_index + 3);
  }

  HRESULT hr = this->mesh.UploadVertexData(device, vertices, 24, sizeof(ShadedVertex));
  if (!SUCCEEDED(hr))
    return hr;

  return this->mesh.UploadIndexData(device, &indices[0], indices.size());
}

//HRESULT CubeObject::Initialize(ID3D11Device1* device) {
//
//}

//HRESULT CubeObject::Initialize(ID3D11Device1* device) {
//  size_t num_slices = 15;
//  size_t num_vertices_per_slice = 15;
//
//  std::vector<ShadedVertex> vertices;
//  for (size_t i = 1; i < num_slices - 1; ++i) {
//    float layer_progress = (float)i / (float)num_slices;
//    float theta = 2 * DirectX::XM_PI * layer_progress;
//
//    for (size_t j = 0; j < num_vertices_per_slice; ++j) {
//      float progress = (float)j / (float)num_vertices_per_slice;
//      float phi = progress * 2 * DirectX::XM_PI;
//
//      ShadedVertex vertex;
//      vertex.pos[0] = std::cos(phi) * std::cos(theta);
//      vertex.pos[1] = std::sin(theta);
//      vertex.pos[2] = std::sin(phi) * std::cos(theta);
//
//      // Probably?
//      vertex.normal[0] = vertex.pos[0];
//      vertex.normal[1] = vertex.pos[1];
//      vertex.normal[2] = vertex.pos[2];
//      vertices.push_back(vertex);
//    }
//  }
//
//  ShadedVertex top;
//  top.pos[0] = 0;
//  top.pos[1] = 1;
//  top.pos[2] = 0;
//  top.normal[0] = 0;
//  top.normal[1] = 1;
//  top.normal[2] = 0;
//
//  ShadedVertex bottom;
//  bottom.pos[0] = 0;
//  bottom.pos[1] = -1;
//  bottom.pos[2] = 0;
//  bottom.normal[0] = 0;
//  bottom.normal[1] = -1;
//  bottom.normal[2] = 0;
//
//  for (size_t i = 0; i < num_slices - 3; ++i) {
//    for (size_t j = 0; j < num_vertices_per_slice; ++j) {
//      // Make the interior triangles.
//    }
//  }
//
//  for (size_t i = 0; i < num_vertices_per_slice; ++i) {
//    // Make triangles for the end caps.
//  }
//
//}
