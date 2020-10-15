#pragma once

#include "d3d12/ObjFileLoader.h"
#include "d3d12/ResourceGarbageCollector.h"

#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

class Model {
  struct Group {
    // TODO: Naming. We probably should have "m_" prefix.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    size_t m_numIndices;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
    D3D12_GPU_DESCRIPTOR_HANDLE m_descriptorHandle;
    // TODO: Add material stuff.
  };

 private:
  Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
  std::vector<Group> m_groups;

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvDescriptorHeap;
  size_t m_srvDescriptorIncrement;

  ObjData::AxisAlignedBounds m_bounds;

  // TODO: This is a lot of parameters. This function should be slimmed down somehow.
  void Init(ID3D12Device* device,
            ID3D12GraphicsCommandList* cl,
            ResourceGarbageCollector& garbageCollector,
            uint64_t nextSignalValue,
            const std::vector<ObjData::Vertex>& vertices,
            const std::vector<ObjData::Group>& objGroups,
            const std::vector<ObjData::Material>& materials);

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
  const ObjData::AxisAlignedBounds& GetBounds() const;
  ID3D12DescriptorHeap* GetSRVDescriptorHeap();

  // TODO: This is so sloppy, should be cleaned up. idk what to, though.
  size_t GetNumberOfGroups();
  D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView(size_t groupIndex);
  D3D12_GPU_DESCRIPTOR_HANDLE GetTextureDescriptorHandle(size_t groupIndex);
  size_t GetNumIndices(size_t groupIndex);
};
