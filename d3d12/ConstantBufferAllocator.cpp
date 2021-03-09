#include "ConstantBufferAllocator.h"

#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

#include <assert.h>

constexpr uint64_t c_pageSize = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
constexpr uint64_t c_bufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

namespace {
Microsoft::WRL::ComPtr<ID3D12Resource> AllocatePage(ID3D12Device* device) {
  D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(c_pageSize);

  Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
  HR(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                     IID_PPV_ARGS(&buffer)));

  return std::move(buffer);
}
}  // namespace

void ConstantBufferAllocator::Initialize(ID3D12Device* device) {
  m_device = device;
}

D3D12_GPU_VIRTUAL_ADDRESS ConstantBufferAllocator::AllocateAndUpload(
    size_t requestedDataSizeInBytes,
    void* data,
    uint64_t nextSignalValue) {
  // We don't currently support constant buffers larger than the page size (64KB).
  assert(requestedDataSizeInBytes <= c_pageSize);
  assert(requestedDataSizeInBytes > 0);

  // If we can't allocate on the current page, mark it as in-flight.
  // TODO: Move this to its own function, maybe MarkCurrentBufferAsInFlight.
  if (m_currentBuffer && (m_currentBufferSignalValue != nextSignalValue ||
                          c_pageSize - m_currentBufferOffset < requestedDataSizeInBytes)) {
    m_currentMappedAddress = nullptr;
    m_currentBuffer->Unmap(0, /*writtenRange*/ nullptr);

    InFlightPage inFlightPage;
    inFlightPage.buffer = std::move(m_currentBuffer);
    inFlightPage.signalValue = m_currentBufferSignalValue;
    m_inFlightPages.emplace_back(std::move(inFlightPage));

    m_currentBufferSignalValue = nextSignalValue;
    m_currentBufferOffset = 0;
  }

  if (!m_currentBuffer) {
    m_currentBuffer = AllocatePage(m_device);
    m_currentBufferOffset = 0;
    m_currentBufferSignalValue = nextSignalValue;
    m_currentMappedAddress = nullptr;

    CD3DX12_RANGE readRange(/*begin*/ 0, /*end*/ 0);
    HR(m_currentBuffer->Map(/*subresource*/ 0, &readRange,
                            reinterpret_cast<void**>(&m_currentMappedAddress)));
  }

  uint8_t* cpuAddress = m_currentMappedAddress + m_currentBufferOffset;
  D3D12_GPU_VIRTUAL_ADDRESS gpuAddress =
      m_currentBuffer->GetGPUVirtualAddress() + m_currentBufferOffset;

  memcpy(cpuAddress, data, requestedDataSizeInBytes);

  // Allocations need to be 256-byte aligned. The easiest way to do that is only increment in
  // multiples of 256.
  size_t alignedDataSize = ((requestedDataSizeInBytes / c_bufferAlignment) + 1) * c_bufferAlignment;
  m_currentBufferOffset += alignedDataSize;
  return gpuAddress;
}

void ConstantBufferAllocator::Cleanup(uint64_t currentSignalValue) {
  // Erase all of the pages that have a signal value that is less than or equal to the current
  // signal value. This will call their destructors and subsequently deallocate their gpu
  // allocation.

  auto firstPageStillInFlightIter = m_inFlightPages.cbegin();
  while (firstPageStillInFlightIter != m_inFlightPages.cend() &&
         firstPageStillInFlightIter->signalValue <= currentSignalValue) {
    firstPageStillInFlightIter++;
  }

  m_inFlightPages.erase(m_inFlightPages.cbegin(), firstPageStillInFlightIter);
}
