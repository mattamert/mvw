#include "PerFrameAllocator.h"

#include <assert.h>

#include "comhelper.h"
#include "d3dx12.h"

using namespace Microsoft::WRL;

ComPtr<ID3D12Resource> PerFrameAllocator::AllocateBuffer(ID3D12Device* device,
                                                         uint64_t signalValue,
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

  if (m_allocations.size() == 0 || m_allocations.back().signalValue != signalValue) {
    assert(m_allocations.size() > 0 ? signalValue > m_allocations.back().signalValue : true);
    m_allocations.emplace();
    m_allocations.back().signalValue = signalValue;
  }

  m_allocations.back().resources.push_back(buffer);

  return buffer;
}

void PerFrameAllocator::Cleanup(uint64_t signalValue) {
  while (m_allocations.size() > 0 && m_allocations.front().signalValue < signalValue)
    m_allocations.pop();
}
