#pragma once

#include "d3d12/ResourceGarbageCollector.h"

#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

class Model {
 private:
  Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
  D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
  size_t m_numIndices;
public:
  void InitCube(ID3D12Device* device,
                ID3D12GraphicsCommandList* cl,
                ResourceGarbageCollector& garbageCollector,
                uint64_t nextSignalValue);

  D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView();
  D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView();
  size_t GetNumIndices();
};
