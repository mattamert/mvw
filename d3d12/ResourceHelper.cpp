#include "d3d12/ResourceHelper.h"

#include "d3d12/d3dx12.h"
#include "d3d12/comhelper.h"

#include <assert.h>

using namespace Microsoft::WRL;

Microsoft::WRL::ComPtr<ID3D12Resource> ResourceHelper::AllocateBuffer(
    ID3D12Device* device,
    unsigned int bytesToAllocate) {
  assert(bytesToAllocate + 255 > bytesToAllocate);  // Make sure that we don't overflow.

  // Buffers must be a multiple of 256 bytes.
  unsigned int numberOf256ByteChunks = (bytesToAllocate + 255) / 256;

  D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(numberOf256ByteChunks * 256);
  ComPtr<ID3D12Resource> buffer;
  HR(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                     IID_PPV_ARGS(&buffer)));

  return buffer;
}
