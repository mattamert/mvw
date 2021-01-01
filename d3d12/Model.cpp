#include "d3d12/Model.h"

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

// TODO: This function is gigantic. Needs to be split up / simplified.
void Model::Init(ID3D12Device* device,
                 ID3D12GraphicsCommandList* cl,
                 ResourceGarbageCollector& garbageCollector,
                 uint64_t nextSignalValue,
                 const std::vector<ObjData::Vertex>& vertices,
                 const std::vector<ObjData::MaterialGroup>& objGroups,
                 const std::vector<ObjData::Material>& materials) {
  // Upload the vertex data.
  const size_t vertexBufferSize = sizeof(ObjData::Vertex) * vertices.size();
  CD3DX12_HEAP_PROPERTIES vertexBufferHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC vertexBufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
  HR(device->CreateCommittedResource(&vertexBufferHeapProperties, D3D12_HEAP_FLAG_NONE,
                                     &vertexBufferResourceDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                     nullptr, IID_PPV_ARGS(&m_vertexBuffer)));

  ComPtr<ID3D12Resource> intermediateVertexBuffer = ResourceHelper::AllocateIntermediateBuffer(
      device, m_vertexBuffer.Get(), garbageCollector, nextSignalValue);

  D3D12_SUBRESOURCE_DATA vertexData;
  vertexData.pData = vertices.data();
  vertexData.RowPitch = vertexBufferSize;
  vertexData.SlicePitch = vertexBufferSize;
  UpdateSubresources(cl, m_vertexBuffer.Get(), intermediateVertexBuffer.Get(), 0, 0, 1,
                     &vertexData);

  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes = vertexBufferSize;
  m_vertexBufferView.StrideInBytes = sizeof(ObjData::Vertex);

  std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
  barriers.reserve(1 + 2 * objGroups.size());
  barriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(
      m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

  // TODO: We may not need all of these descriptors, but just for now, let's just over-allocate and
  // worry about cleaning it up later.
  D3D12_DESCRIPTOR_HEAP_DESC srvDescriptorHeapDesc;
  srvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  srvDescriptorHeapDesc.NumDescriptors = objGroups.size();
  srvDescriptorHeapDesc.NodeMask = 0;
  HR(device->CreateDescriptorHeap(&srvDescriptorHeapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap)));
  size_t incrementSize =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  m_srvDescriptorIncrement = incrementSize;

  CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(
      m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
  CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(
      m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

  // Upload the ObjFile groups.
  m_groups.resize(objGroups.size());
  for (size_t i = 0; i < objGroups.size(); ++i) {
    const ObjData::MaterialGroup& objGroup = objGroups[i];
    Model::Group& modelGroup = m_groups[i];

    // Upload the index data.
    const size_t indexBufferSize = objGroup.indices.size() * sizeof(uint32_t);
    CD3DX12_HEAP_PROPERTIES indexBufferHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC indexBufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
    HR(device->CreateCommittedResource(&indexBufferHeapProperties, D3D12_HEAP_FLAG_NONE,
      &indexBufferResourceDesc, D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr, IID_PPV_ARGS(&modelGroup.m_indexBuffer)));

    ComPtr<ID3D12Resource> intermediateIndexBuffer = ResourceHelper::AllocateIntermediateBuffer(
      device, modelGroup.m_indexBuffer.Get(), garbageCollector, nextSignalValue);

    D3D12_SUBRESOURCE_DATA indexData;
    indexData.pData = objGroup.indices.data();
    indexData.RowPitch = indexBufferSize;
    indexData.SlicePitch = indexBufferSize;
    UpdateSubresources(cl, modelGroup.m_indexBuffer.Get(), intermediateIndexBuffer.Get(), 0, 0, 1, &indexData);

    modelGroup.m_numIndices = objGroup.indices.size();
    modelGroup.m_indexBufferView.BufferLocation = modelGroup.m_indexBuffer->GetGPUVirtualAddress();
    modelGroup.m_indexBufferView.SizeInBytes = indexBufferSize;
    modelGroup.m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    barriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(modelGroup.m_indexBuffer.Get(),
                                                               D3D12_RESOURCE_STATE_COPY_DEST,
                                                               D3D12_RESOURCE_STATE_GENERIC_READ));

    // TODO: We should check if we've already uploaded the file, and then just use that one.
    // This will get much more relevant when we factor in other of mtl's maps.
    modelGroup.m_texture = nullptr;
    if (objGroup.materialIndex >= 0) {
      const ObjData::Material& material = materials[objGroup.materialIndex];
      if (std::filesystem::exists(material.diffuseMap.file)) {
        Image img;
        HR(Image::LoadImageFile(material.diffuseMap.file.wstring(), &img));

        // Upload the texture.
        const size_t imgBufferSize = img.data.size();
        CD3DX12_HEAP_PROPERTIES textureResourceHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            img.format, img.width, img.height, /*arraySize*/ 1, /*mipLevels*/ 1);
        HR(device->CreateCommittedResource(&textureResourceHeapProperties, D3D12_HEAP_FLAG_NONE,
                                           &textureResourceDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                           nullptr, IID_PPV_ARGS(&modelGroup.m_texture)));

        ComPtr<ID3D12Resource> intermediateTextureBuffer =
            ResourceHelper::AllocateIntermediateBuffer(device, modelGroup.m_texture.Get(),
                                                       garbageCollector, nextSignalValue);

        D3D12_SUBRESOURCE_DATA textureData;
        textureData.pData = img.data.data();
        textureData.RowPitch = img.bytesPerPixel * img.width;
        textureData.SlicePitch = img.bytesPerPixel * img.width * img.height;
        UpdateSubresources(cl, modelGroup.m_texture.Get(), intermediateTextureBuffer.Get(), 0, 0, 1,
                           &textureData);

        barriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(
            modelGroup.m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = img.format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0;
        device->CreateShaderResourceView(modelGroup.m_texture.Get(), &srvDesc, cpuHandle);
        modelGroup.m_srvDescriptorHandle = gpuHandle;

        cpuHandle.Offset(incrementSize);
        gpuHandle.Offset(incrementSize);
      }
    }
  }

  cl->ResourceBarrier(barriers.size(), barriers.data());
}

void Model::InitCube(ID3D12Device* device,
                     ID3D12GraphicsCommandList* cl,
                     ResourceGarbageCollector& garbageCollector,
                     uint64_t nextSignalValue) {
  std::vector<ObjData::Vertex> vertices = {
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

  std::vector<ObjData::MaterialGroup> group(1);
  group[0].indices = std::move(indices);
  group[0].materialIndex = -1;

  // TODO: Pretty sure this will crash without a material. Should probably fix (and clean everything
  // up...).
  std::vector<ObjData::Material> materials;

  // TODO: Generate generic material.
  Init(device, cl, garbageCollector, nextSignalValue, vertices, group, materials);
}

bool Model::InitFromObjFile(ID3D12Device* device,
                            ID3D12GraphicsCommandList* cl,
                            ResourceGarbageCollector& garbageCollector,
                            uint64_t nextSignalValue,
                            const std::string& fileName) {
  ObjData data;
  if (!data.ParseObjFile(fileName)) {
    return false;
  }

  m_bounds = data.m_bounds;
  Init(device, cl, garbageCollector, nextSignalValue, data.m_vertices, data.m_groups,
       data.m_materials);

  return true;
}

D3D12_VERTEX_BUFFER_VIEW& Model::GetVertexBufferView() {
  return m_vertexBufferView;
}


const ObjData::AxisAlignedBounds& Model::GetBounds() const {
  return m_bounds;
}

ID3D12DescriptorHeap* Model::GetSRVDescriptorHeap() {
  return m_srvDescriptorHeap.Get();
}

size_t Model::GetNumberOfGroups() {
  return m_groups.size();
}

D3D12_INDEX_BUFFER_VIEW& Model::GetIndexBufferView(size_t groupIndex) {
  return m_groups[groupIndex].m_indexBufferView;
}

D3D12_GPU_DESCRIPTOR_HANDLE Model::GetTextureDescriptorHandle(size_t groupIndex) {
  return m_groups[groupIndex].m_srvDescriptorHandle;
}

size_t Model::GetNumIndices(size_t groupIndex) {
  return m_groups[groupIndex].m_numIndices;
}
