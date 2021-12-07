#include "d3d12/Model.h"

#include "d3d12/D3D12Renderer.h"
#include "d3d12/ImageLoader.h"
#include "d3d12/ObjFileLoader.h"
#include "d3d12/Pass.h"
#include "d3d12/ResourceHelper.h"
#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

#include <assert.h>
#include <filesystem>
#include <iostream>
#include <vector>

#include "d3d12/ObjFileLoader.h"

#include <wrl/client.h>  // For ComPtr

using namespace Microsoft::WRL;

void Model::Init(D3D12Renderer* renderer,
                 const std::vector<ObjFileData::Vertex>& vertices,
                 const std::vector<ObjFileData::MaterialGroup>& objGroups,
                 const std::vector<ObjFileData::Material>& materials) {
  std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
  barriers.reserve(1 + 2 * objGroups.size());  // TODO: update the estimated size when we handle multiple groups with the
                                               //       same texture data correctly.

  // Upload the vertex data.
  const size_t vertexBufferSize = vertices.size() * sizeof(ObjFileData::Vertex);
  m_vertexBuffer = renderer->AllocateAndUploadBufferData(vertices.data(), vertexBufferSize);
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                                          D3D12_RESOURCE_STATE_GENERIC_READ));

  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes = vertexBufferSize;
  m_vertexBufferView.StrideInBytes = sizeof(ObjFileData::Vertex);

  // Upload each object group.
  // TODO: We need to upload the textures in a separate step; multiple groups could have the same texture.
  m_groups.resize(objGroups.size());
  for (size_t i = 0; i < objGroups.size(); ++i) {
    const ObjFileData::MaterialGroup& objGroup = objGroups[i];
    Model::Group& modelGroup = m_groups[i];

    // Upload the index data.
    const size_t indexBufferSize = objGroup.indices.size() * sizeof(uint32_t);
    modelGroup.m_indexBuffer = renderer->AllocateAndUploadBufferData(objGroup.indices.data(), indexBufferSize);
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
        modelGroup.m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

    modelGroup.m_numIndices = objGroup.indices.size();
    modelGroup.m_indexBufferView.BufferLocation = modelGroup.m_indexBuffer->GetGPUVirtualAddress();
    modelGroup.m_indexBufferView.SizeInBytes = indexBufferSize;
    modelGroup.m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    // TODO: We should check if we've already uploaded the file, and then just use that one.
    //       This will get much more relevant when we factor in other of mtl's maps.
    modelGroup.m_texture = nullptr;
    if (objGroup.materialIndex >= 0) {
      const ObjFileData::Material& material = materials[objGroup.materialIndex];
      if (std::filesystem::exists(material.diffuseMap.file)) {
        Image img;
        HR(Image::LoadImageFile(material.diffuseMap.file.wstring(), &img));

        // Upload the texture.
        modelGroup.m_texture = renderer->AllocateAndUploadTextureData(img.data.data(), img.format, img.bytesPerPixel,
                                                                      img.width, img.height,
                                                                      /*out*/ &modelGroup.m_srvDescriptor);
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            modelGroup.m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
      }
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

  std::vector<ObjFileData::MaterialGroup> group(1);
  group[0].indices = std::move(indices);
  group[0].materialIndex = -1;

  // TODO: Pretty sure this will crash without a material. Should probably fix (and clean everything
  // up...).
  std::vector<ObjFileData::Material> materials;

  // TODO: Generate generic material.
  Init(renderer, vertices, group, materials);
}

bool Model::InitFromObjFile(D3D12Renderer* renderer, const std::string& fileName) {
  ObjFileData data;
  if (!data.ParseObjFile(fileName)) {
    return false;
  }

  m_bounds = data.m_bounds;
  Init(renderer, data.m_vertices, data.m_groups, data.m_materials);
  return true;
}

D3D12_VERTEX_BUFFER_VIEW& Model::GetVertexBufferView() {
  return m_vertexBufferView;
}

const ObjFileData::AxisAlignedBounds& Model::GetBounds() const {
  return m_bounds;
}

size_t Model::GetNumberOfGroups() {
  return m_groups.size();
}

D3D12_INDEX_BUFFER_VIEW& Model::GetIndexBufferView(size_t groupIndex) {
  return m_groups[groupIndex].m_indexBufferView;
}

D3D12_CPU_DESCRIPTOR_HANDLE Model::GetTextureDescriptorHandle(size_t groupIndex) {
  return m_groups[groupIndex].m_srvDescriptor.cpuStart;
}

size_t Model::GetNumIndices(size_t groupIndex) {
  return m_groups[groupIndex].m_numIndices;
}
