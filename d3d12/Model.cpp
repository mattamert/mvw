#include "d3d12/Model.h"

#include "d3d12/Pass.h"
#include "d3d12/ResourceHelper.h"
#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

#include <assert.h>
#include <vector>

#include <wrl/client.h>  // For ComPtr

using namespace Microsoft::WRL;

void Model::InitCube(ID3D12Device* device,
                     ID3D12GraphicsCommandList* cl,
                     ResourceGarbageCollector& garbageCollector,
                     uint64_t nextSignalValue) {
  ColorPass::VertexData vertices[] = {
      // +x direction
      {+1.0f, -1.0f, -1.0f, 1.f, 0.f, 0.f},
      {+1.0f, +1.0f, -1.0f, 1.f, 0.f, 0.f},
      {+1.0f, +1.0f, +1.0f, 1.f, 0.f, 0.f},
      {+1.0f, -1.0f, +1.0f, 1.f, 0.f, 0.f},

      // -x direction
      {-1.0f, -1.0f, +1.0f, -1.f, 0.f, 0.f},
      {-1.0f, +1.0f, +1.0f, -1.f, 0.f, 0.f},
      {-1.0f, +1.0f, -1.0f, -1.f, 0.f, 0.f},
      {-1.0f, -1.0f, -1.0f, -1.f, 0.f, 0.f},

      // +y direction
      {+1.0f, +1.0f, +1.0f, 0.f, 1.f, 0.f},
      {+1.0f, +1.0f, -1.0f, 0.f, 1.f, 0.f},
      {-1.0f, +1.0f, -1.0f, 0.f, 1.f, 0.f},
      {-1.0f, +1.0f, +1.0f, 0.f, 1.f, 0.f},

      // -y direction
      {+1.0f, -1.0f, +1.0f, 0.f, -1.f, 0.f},
      {+1.0f, -1.0f, -1.0f, 0.f, -1.f, 0.f},
      {-1.0f, -1.0f, -1.0f, 0.f, -1.f, 0.f},
      {-1.0f, -1.0f, +1.0f, 0.f, -1.f, 0.f},

      // +z direction
      {+1.0f, -1.0f, +1.0f, 0.f, 0.f, 1.f},
      {+1.0f, +1.0f, +1.0f, 0.f, 0.f, 1.f},
      {-1.0f, +1.0f, +1.0f, 0.f, 0.f, 1.f},
      {-1.0f, -1.0f, +1.0f, 0.f, 0.f, 1.f},

      // -z direction
      {-1.0f, -1.0f, -1.0f, 0.f, 0.f, -1.f},
      {-1.0f, +1.0f, -1.0f, 0.f, 0.f, -1.f},
      {+1.0f, +1.0f, -1.0f, 0.f, 0.f, -1.f},
      {+1.0f, -1.0f, -1.0f, 0.f, 0.f, -1.f},
  };

  std::vector<uint32_t> indices;
  indices.reserve(6 * 6);
  for (size_t i = 0; i < 6; ++i) {
    uint32_t starting_index = i * 4;
    indices.push_back(starting_index);
    indices.push_back(starting_index + 1);
    indices.push_back(starting_index + 2);

    indices.push_back(starting_index);
    indices.push_back(starting_index + 2);
    indices.push_back(starting_index + 3);
  }

  // Upload the vertex data.
  const size_t vertexBufferSize = sizeof(vertices);
  CD3DX12_HEAP_PROPERTIES vertexBufferHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC vertexBufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
  HR(device->CreateCommittedResource(&vertexBufferHeapProperties, D3D12_HEAP_FLAG_NONE,
                                     &vertexBufferResourceDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                     nullptr, IID_PPV_ARGS(&m_vertexBuffer)));

  ComPtr<ID3D12Resource> intermediateVertexBuffer = ResourceHelper::AllocateIntermediateBuffer(
      device, m_vertexBuffer.Get(), garbageCollector, nextSignalValue);

  D3D12_SUBRESOURCE_DATA vertexData;
  vertexData.pData = vertices;
  vertexData.RowPitch = vertexBufferSize;
  vertexData.SlicePitch = vertexBufferSize;
  UpdateSubresources(cl, m_vertexBuffer.Get(), intermediateVertexBuffer.Get(), 0, 0, 1,
                     &vertexData);

  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes = vertexBufferSize;
  m_vertexBufferView.StrideInBytes = sizeof(ColorPass::VertexData);

  // Upload the index data.
  const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
  CD3DX12_HEAP_PROPERTIES indexBufferHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC indexBufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
  HR(device->CreateCommittedResource(&indexBufferHeapProperties, D3D12_HEAP_FLAG_NONE,
                                     &indexBufferResourceDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                     nullptr, IID_PPV_ARGS(&m_indexBuffer)));

  ComPtr<ID3D12Resource> intermediateIndexBuffer = ResourceHelper::AllocateIntermediateBuffer(
      device, m_indexBuffer.Get(), garbageCollector, nextSignalValue);

  D3D12_SUBRESOURCE_DATA indexData;
  indexData.pData = indices.data();
  indexData.RowPitch = indexBufferSize;
  indexData.SlicePitch = indexBufferSize;
  UpdateSubresources(cl, m_indexBuffer.Get(), intermediateIndexBuffer.Get(), 0, 0, 1, &indexData);

  m_numIndices = indices.size();
  m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
  m_indexBufferView.SizeInBytes = indexBufferSize;
  m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

  CD3DX12_RESOURCE_BARRIER barriers[2] = {
      CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                           D3D12_RESOURCE_STATE_GENERIC_READ),
      CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                           D3D12_RESOURCE_STATE_GENERIC_READ),
  };
  cl->ResourceBarrier(1, barriers);
}

D3D12_VERTEX_BUFFER_VIEW& Model::GetVertexBufferView() {
  return m_vertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW& Model::GetIndexBufferView() {
  return m_indexBufferView;
}

size_t Model::GetNumIndices() {
  return m_numIndices;
}
