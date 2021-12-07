#pragma once

#include "d3d12/DescriptorHeapManagers.h"
#include "d3d12/ObjFileLoader.h"
#include "d3d12/ResourceGarbageCollector.h"

#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

class D3D12Renderer;

class Model {
 private:
  struct Group {
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    size_t m_numIndices;

    // TODO: Split this off into a separate class that could be referenced by several groups.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
    DescriptorAllocation m_srvDescriptor;
  };

  Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
  std::vector<Group> m_groups;

  ObjFileData::AxisAlignedBounds m_bounds;

  void Init(D3D12Renderer* renderer,
            const std::vector<ObjFileData::Vertex>& vertices,
            const std::vector<ObjFileData::MaterialGroup>& objGroups,
            const std::vector<ObjFileData::Material>& materials);

 public:
  void InitCube(D3D12Renderer* renderer);
  bool InitFromObjFile(D3D12Renderer* renderer, const std::string& fileName);

  D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView();
  const ObjFileData::AxisAlignedBounds& GetBounds() const;

  // TODO: This is so sloppy, should be cleaned up. idk what to, though.
  size_t GetNumberOfGroups();
  D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView(size_t groupIndex);
  D3D12_CPU_DESCRIPTOR_HANDLE GetTextureDescriptorHandle(size_t groupIndex);
  size_t GetNumIndices(size_t groupIndex);
};
