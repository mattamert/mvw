#include "DescriptorHeapManagers.h"

#include "comhelper.h"
#include "d3dx12.h"

#include <assert.h>

namespace {
// Arbitrary number. Likely needs tuning.
constexpr unsigned int c_linearDescriptorHeapSize = 100;
}  // namespace

LinearDescriptorAllocator::LinearDescriptorAllocator()
    : m_device(nullptr), m_descriptorHeap(nullptr), m_descriptorIncrementSize(0u) {}

void LinearDescriptorAllocator::Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type) {
  m_device = device;
  m_heapType = type;
  m_descriptorIncrementSize = m_device->GetDescriptorHandleIncrementSize(type);
}

D3D12_CPU_DESCRIPTOR_HANDLE LinearDescriptorAllocator::AllocateSingleDescriptor() {
  assert(m_device);
  assert(m_descriptorIncrementSize > 0u);

  if (!m_descriptorHeap || m_currentIndex >= m_currentHeapSize) {
    // Allocate new heap.
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
    heapDesc.Type = m_heapType;
    heapDesc.NumDescriptors = 100; 
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // Not shader visible.
    heapDesc.NodeMask = 0;
    HR(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));

    m_currentHeapSize = c_linearDescriptorHeapSize;
    m_currentIndex = 0;

    m_heapStart = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
  }

  CD3DX12_CPU_DESCRIPTOR_HANDLE allocatedDescriptor(m_heapStart, m_currentIndex, m_descriptorIncrementSize);
  m_currentIndex++;
  return allocatedDescriptor;
}
