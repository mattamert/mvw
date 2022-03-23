#pragma once

#include "d3d12/DescriptorHeapManagers.h"
#include "d3d12/ObjFileLoader.h"

#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

#include <vector>

class D3D12Renderer;

struct Model {
  struct Material {
    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
    DescriptorAllocation m_srvDescriptor;
  };

  Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
  D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

  std::vector<ObjFileData::MeshPart> m_meshParts;
  std::vector<Material> m_materials;
  ObjFileData::AxisAlignedBounds m_bounds;

  void Init(D3D12Renderer* renderer,
            const std::vector<ObjFileData::Vertex>& vertices,
            const std::vector<uint32_t>& indices,
            const std::vector<ObjFileData::MeshPart>& meshParts,
            const std::vector<ObjFileData::Material>& materials);

  void InitCube(D3D12Renderer* renderer);
  bool InitFromObjFile(D3D12Renderer* renderer, const std::string& fileName);

  D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView();
  const ObjFileData::AxisAlignedBounds& GetBounds() const;
};
