#pragma once

#include "d3d12/ObjFileLoader.h"
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

  Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvDescriptorHeap;

  // TODO: This is a lot of parameters. This function should be slimmed down somehow.
  void Init(ID3D12Device* device,
            ID3D12GraphicsCommandList* cl,
            ResourceGarbageCollector& garbageCollector,
            uint64_t nextSignalValue,
            const std::vector<ObjData::Vertex>& vertices,
            const std::vector<uint32_t>& indices,
            const ObjData::Material* material);

public:
  void InitCube(ID3D12Device* device,
                ID3D12GraphicsCommandList* cl,
                ResourceGarbageCollector& garbageCollector,
                uint64_t nextSignalValue);

  bool InitFromObjFile(ID3D12Device* device,
                       ID3D12GraphicsCommandList* cl,
                       ResourceGarbageCollector& garbageCollector,
                       uint64_t nextSignalValue,
                       const std::string& fileName);

  D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView();
  D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView();
  size_t GetNumIndices();

  ID3D12DescriptorHeap* GetSRVDescriptorHeap();
};
