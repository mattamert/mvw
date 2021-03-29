#include "DescriptorHeapManagers.h"

#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

#include <assert.h>

namespace {
// Arbitrary numbers. Needs tuning!
constexpr unsigned int c_linearDescriptorHeapSize = 100;
constexpr unsigned int c_circularBufferDescriptorHeapSize = 100;
}  // namespace

void LinearDescriptorAllocator::Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type) {
  m_device = device;
  m_heapType = type;
  m_descriptorIncrementSize = m_device->GetDescriptorHandleIncrementSize(type);
}

DescriptorAllocation LinearDescriptorAllocator::AllocateSingleDescriptor() {
  assert(m_device);
  assert(m_descriptorIncrementSize > 0u);

  if (!m_descriptorHeap || m_currentIndex >= m_currentHeapSize) {
    // Allocate new heap.
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
    heapDesc.Type = m_heapType;
    heapDesc.NumDescriptors = c_linearDescriptorHeapSize;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // Not shader visible.
    heapDesc.NodeMask = 0;
    HR(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));

    m_currentHeapSize = c_linearDescriptorHeapSize;
    m_currentIndex = 0;

    m_heapStart = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
  }

  size_t allocatedIndex = m_currentIndex;
  CD3DX12_CPU_DESCRIPTOR_HANDLE allocatedDescriptorCPU(m_heapStart, allocatedIndex, m_descriptorIncrementSize);
  CD3DX12_GPU_DESCRIPTOR_HANDLE allocatedDescriptorGPU(D3D12_DEFAULT);
  m_currentIndex++;
  return DescriptorAllocation{allocatedDescriptorCPU, allocatedDescriptorGPU, 1ull, allocatedIndex};
}

void CircularBufferDescriptorAllocator::Initialize(ID3D12Device* device,
                                                   D3D12_DESCRIPTOR_HEAP_TYPE type) {
  m_device = device;
  m_heapType = type;
  m_descriptorIncrementSize = m_device->GetDescriptorHandleIncrementSize(type);

  // Allocate new heap.
  D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
  heapDesc.Type = m_heapType;
  heapDesc.NumDescriptors = c_circularBufferDescriptorHeapSize;
  heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  heapDesc.NodeMask = 0;
  HR(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));

  m_heapSize = c_circularBufferDescriptorHeapSize;
  m_firstFreeIndex = 0;
  m_numFreeDescriptors = m_heapSize;

  m_heapStartCPU = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
  m_heapStartGPU = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

ID3D12DescriptorHeap* CircularBufferDescriptorAllocator::GetDescriptorHeap() {
  return m_descriptorHeap.Get();
}

DescriptorAllocation CircularBufferDescriptorAllocator::AllocateSingleDescriptor(
    size_t nextSignalValue) {
  assert(m_numFreeDescriptors > 0);
  if (m_currentAllocation.has_value()) {
    // Assume that the signal value is monotomically increasing.
    assert(m_currentAllocation->signalValue <= nextSignalValue);
  }

  if (m_currentAllocation.has_value() && m_currentAllocation->signalValue < nextSignalValue) {
    m_inFlightDescriptorRanges.emplace(std::move(*m_currentAllocation));
    m_currentAllocation.reset();
  }

  if (!m_currentAllocation) {
    m_currentAllocation.emplace();
    m_currentAllocation->signalValue = nextSignalValue;
    m_currentAllocation->startIndex = m_firstFreeIndex;
    m_currentAllocation->numDescriptors = 0;
  }

  size_t allocatedIndex = m_firstFreeIndex;
  CD3DX12_CPU_DESCRIPTOR_HANDLE allocatedDescriptorCPU(m_heapStartCPU, m_firstFreeIndex, m_descriptorIncrementSize);
  CD3DX12_GPU_DESCRIPTOR_HANDLE allocatedDescriptorGPU(m_heapStartGPU, m_firstFreeIndex, m_descriptorIncrementSize);
  m_currentAllocation->numDescriptors++;
  m_firstFreeIndex = (m_firstFreeIndex + 1) % m_heapSize;
  --m_numFreeDescriptors;

  return DescriptorAllocation{allocatedDescriptorCPU, allocatedDescriptorGPU, 1ull, allocatedIndex};
}

void CircularBufferDescriptorAllocator::Cleanup(size_t signalValue) {
  if (m_currentAllocation.has_value() && m_currentAllocation->signalValue <= signalValue) {
    m_inFlightDescriptorRanges.emplace(std::move(*m_currentAllocation));
    m_currentAllocation.reset();
  }

  while (m_inFlightDescriptorRanges.size() > 0 &&
         m_inFlightDescriptorRanges.front().signalValue <= signalValue) {

    assert(m_inFlightDescriptorRanges.front().startIndex ==
           (m_firstFreeIndex + m_numFreeDescriptors) % m_heapSize);

    m_numFreeDescriptors += m_inFlightDescriptorRanges.front().numDescriptors;
    m_inFlightDescriptorRanges.pop();
  }
}
