#include "GPUMesh.h"

#include <DirectXMath.h>
#include "Helper.h"

namespace {
enum class BufferType {
  Vertex,
  Index
};

HRESULT CreateBuffer(ID3D11Device1* device, void* initial_data, int bytes_to_allocate, BufferType buffer_type, /*out*/ID3D11Buffer** buffer) {
  // Currently assumes that we want an immutable buffer that the CPU shouldn't access.
  D3D11_BUFFER_DESC vbd;
  vbd.ByteWidth = bytes_to_allocate;
  vbd.Usage = D3D11_USAGE_IMMUTABLE;
  vbd.BindFlags = (buffer_type == BufferType::Vertex) ? D3D11_BIND_VERTEX_BUFFER : D3D11_BIND_INDEX_BUFFER;
  vbd.CPUAccessFlags = 0; // Specify 0 if the cpu doesn't need to access it.
  vbd.MiscFlags = 0; // See D3D11_RESOURCE_MISC_FLAG for more details.
  vbd.StructureByteStride = 0; // Only applies to structured buffers.

  D3D11_SUBRESOURCE_DATA subresource_data;
  subresource_data.pSysMem = initial_data;
  subresource_data.SysMemPitch = 0; // (Not used for vertex or index buffers)
  subresource_data.SysMemSlicePitch = 0; // (Not used for vertex or index buffers)

  return device->CreateBuffer(&vbd, &subresource_data, buffer);
}
}

GPUMesh::~GPUMesh() {
  SafeRelease(&vertex_buffer_);
  SafeRelease(&index_buffer_);
}

HRESULT GPUMesh::UploadVertexData(ID3D11Device1* device, void* vertex_data, int vertex_count, int bytes_per_vertex) {
  this->num_vertices = vertex_count;
  return CreateBuffer(device, vertex_data, vertex_count * bytes_per_vertex, BufferType::Vertex, &vertex_buffer_);
}

HRESULT GPUMesh::UploadIndexData(ID3D11Device1* device, uint32_t* index_data, int count) {
  this->num_indices = count;
  return CreateBuffer(device, index_data, sizeof(uint32_t) * count, BufferType::Vertex, &index_buffer_);
}

