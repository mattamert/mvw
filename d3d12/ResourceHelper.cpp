#include "d3d12/ResourceHelper.h"

#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

#include <assert.h>

using namespace Microsoft::WRL;

Microsoft::WRL::ComPtr<ID3D12Resource> ResourceHelper::AllocateBuffer(
    ID3D12Device* device,
    unsigned int bytesToAllocate) {
  // TODO: We could probably save some space if we do some smart alignment here.
  //       Constant buffers are likely not to use the full 64kB aligned. We should be able to reduce
  //       the alignment to 256.
  D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bytesToAllocate);
  ComPtr<ID3D12Resource> buffer;
  HR(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                     IID_PPV_ARGS(&buffer)));

  return buffer;
}

Microsoft::WRL::ComPtr<ID3D12Resource> ResourceHelper::AllocateIntermediateBuffer(
    ID3D12Device* device,
    ID3D12Resource* destinationResource,
    ResourceGarbageCollector& garbageCollector,
    uint64_t nextSignalValue) {
  const uint64_t uploadBufferSize = GetRequiredIntermediateSize(destinationResource, 0, 1);
  D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

  ComPtr<ID3D12Resource> intermediateBuffer;
  HR(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                     IID_PPV_ARGS(&intermediateBuffer)));

  garbageCollector.MarkAsGarbage(intermediateBuffer.Get(), nextSignalValue);

  return intermediateBuffer;
}

void ResourceHelper::UpdateBuffer(ID3D12Resource* buffer, void* newData, size_t dataSize) {
  assert(buffer != nullptr);

  uint8_t* mappedRegion;
  CD3DX12_RANGE readRange(0, 0);
  HR(buffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedRegion)));
  memcpy(mappedRegion, newData, dataSize);
  buffer->Unmap(0, nullptr);
}
