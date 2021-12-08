#include "d3d12/Model.h"

#include "d3d12/comhelper.h"
#include "d3d12/D3D12Renderer.h"
#include "d3d12/d3dx12.h"
#include "d3d12/ImageLoader.h"
#include "d3d12/ObjFileLoader.h"

#include <wrl/client.h>  // For ComPtr

#include <assert.h>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace Microsoft::WRL;

void Model::Init(D3D12Renderer* renderer,
                 const std::vector<ObjFileData::Vertex>& vertices,
                 const std::vector<uint32_t>& indices,
                 const std::vector<ObjFileData::MeshPart>& meshParts,
                 const std::vector<ObjFileData::Material>& materials) {
  std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
  barriers.reserve(2 + materials.size());  // 1 for vertex buffer, 1 for index buffer, plus all of the materials.

  // Upload the vertex data.
  const size_t vertexBufferSize = vertices.size() * sizeof(ObjFileData::Vertex);
  m_vertexBuffer = renderer->AllocateAndUploadBufferData(vertices.data(), vertexBufferSize);
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_GENERIC_READ));

  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes = vertexBufferSize;
  m_vertexBufferView.StrideInBytes = sizeof(ObjFileData::Vertex);

  // Upload the index data.
  const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
  m_indexBuffer = renderer->AllocateAndUploadBufferData(indices.data(), indexBufferSize);
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                                          D3D12_RESOURCE_STATE_GENERIC_READ));

  m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
  m_indexBufferView.SizeInBytes = indexBufferSize;
  m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

  // Just copy over the meshPart data.
  m_meshParts = meshParts;

  // Upload all of the texture data.
  m_materials.resize(materials.size());
  for (size_t i = 0; i < materials.size(); ++i) {
    const ObjFileData::Material& material = materials[i];
    if (std::filesystem::exists(material.diffuseMap.file)) {
      Image img;
      HR(Image::LoadImageFile(material.diffuseMap.file.wstring(), &img));

      // Upload the texture.
      m_materials[i].m_texture =
          renderer->AllocateAndUploadTextureData(img.data.data(), img.format, img.bytesPerPixel, img.width, img.height,
                                                 /*out*/ &m_materials[i].m_srvDescriptor);
      barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
          m_materials[i].m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    }
  }

  renderer->ExecuteBarriers(barriers.size(), barriers.data());
}

void Model::InitCube(D3D12Renderer* renderer) {
  std::vector<ObjFileData::Vertex> vertices = {
      // +x direction
      {{+1.0f, +1.0f, -1.0f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
      {{+1.0f, +1.0f, +1.0f}, {1.f, 0.f}, {1.f, 0.f, 0.f}},
      {{+1.0f, -1.0f, +1.0f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
      {{+1.0f, -1.0f, -1.0f}, {0.f, 1.f}, {1.f, 0.f, 0.f}},

      // -x direction
      {{-1.0f, -1.0f, +1.0f}, {0.f, 0.f}, {-1.f, 0.f, 0.f}},
      {{-1.0f, -1.0f, -1.0f}, {1.f, 0.f}, {-1.f, 0.f, 0.f}},
      {{-1.0f, +1.0f, -1.0f}, {1.f, 1.f}, {-1.f, 0.f, 0.f}},
      {{-1.0f, +1.0f, +1.0f}, {0.f, 1.f}, {-1.f, 0.f, 0.f}},

      // +y direction
      {{-1.0f, +1.0f, +1.0f}, {0.f, 0.f}, {0.f, 1.f, 0.f}},
      {{+1.0f, +1.0f, +1.0f}, {1.f, 0.f}, {0.f, 1.f, 0.f}},
      {{+1.0f, +1.0f, -1.0f}, {1.f, 1.f}, {0.f, 1.f, 0.f}},
      {{-1.0f, +1.0f, -1.0f}, {0.f, 1.f}, {0.f, 1.f, 0.f}},

      // -y direction
      {{-1.0f, -1.0f, -1.0f}, {0.f, 0.f}, {0.f, -1.f, 0.f}},
      {{+1.0f, -1.0f, -1.0f}, {1.f, 0.f}, {0.f, -1.f, 0.f}},
      {{+1.0f, -1.0f, +1.0f}, {1.f, 1.f}, {0.f, -1.f, 0.f}},
      {{-1.0f, -1.0f, +1.0f}, {0.f, 1.f}, {0.f, -1.f, 0.f}},

      // +z direction
      {{+1.0f, +1.0f, +1.0f}, {0.f, 0.f}, {0.f, 0.f, 1.f}},
      {{-1.0f, +1.0f, +1.0f}, {1.f, 0.f}, {0.f, 0.f, 1.f}},
      {{-1.0f, -1.0f, +1.0f}, {1.f, 1.f}, {0.f, 0.f, 1.f}},
      {{+1.0f, -1.0f, +1.0f}, {0.f, 1.f}, {0.f, 0.f, 1.f}},

      // -z direction
      {{-1.0f, +1.0f, -1.0f}, {0.f, 0.f}, {0.f, 0.f, -1.f}},
      {{+1.0f, +1.0f, -1.0f}, {1.f, 0.f}, {0.f, 0.f, -1.f}},
      {{+1.0f, -1.0f, -1.0f}, {1.f, 1.f}, {0.f, 0.f, -1.f}},
      {{-1.0f, -1.0f, -1.0f}, {0.f, 1.f}, {0.f, 0.f, -1.f}},
  };

  std::vector<uint32_t> indices;
  indices.reserve(6 * 6);  // 6 indices per face * 6 faces.
  for (size_t i = 0; i < 6; ++i) {
    uint32_t starting_index = i * 4;
    indices.push_back(starting_index);
    indices.push_back(starting_index + 1);
    indices.push_back(starting_index + 2);

    indices.push_back(starting_index);
    indices.push_back(starting_index + 2);
    indices.push_back(starting_index + 3);
  }

  for (size_t i = 0; i < 3; ++i) {
    m_bounds.max[i] = 1.0;
    m_bounds.min[i] = -1.0;
  }

  std::vector<ObjFileData::MeshPart> meshParts(1);
  meshParts[0].indexStart = 0;
  meshParts[0].numIndices = indices.size();
  meshParts[0].materialIndex = -1;

  // TODO: Pretty sure this will crash without a material. Should probably generate a generic material.
  //       (Or handle the case better where we don't have a material).
  std::vector<ObjFileData::Material> materials;
  Init(renderer, vertices, indices, meshParts, materials);
}

bool Model::InitFromObjFile(D3D12Renderer* renderer, const std::string& fileName) {
  ObjFileData data;
  if (!data.ParseObjFile(fileName)) {
    return false;
  }

  m_bounds = data.m_bounds;
  Init(renderer, data.m_vertices, data.m_indices, data.m_meshParts, data.m_materials);
  return true;
}

D3D12_VERTEX_BUFFER_VIEW& Model::GetVertexBufferView() {
  return m_vertexBufferView;
}

const ObjFileData::AxisAlignedBounds& Model::GetBounds() const {
  return m_bounds;
}
