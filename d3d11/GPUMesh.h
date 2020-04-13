#pragma once

#include <d3d11_1.h>
#include <DirectXMath.h>

class GPUMesh {
public:
  ~GPUMesh();
  HRESULT UploadVertexData(ID3D11Device1* device, void* vertex_data, int vertex_count, int bytes_per_vertex);
  HRESULT UploadIndexData(ID3D11Device1* device, uint32_t* index_data, int index_count);

  ID3D11Buffer* vertex_buffer_ = nullptr;
  ID3D11Buffer* index_buffer_ = nullptr;
  unsigned int num_vertices;
  unsigned int num_indices;
};
