#pragma once

#include <d3d12.h>
#include <optional>
#include <queue>
#include <wrl/client.h>  // For ComPtr

// The strategy for managing descriptors:
//  - keep long-term copies of descriptors in non-shader-visible descriptor heaps, which are managed
//    by some sort of allocator (freelist is likely the most ideal for this).
//    Consumers will hold a handle to a descriptor in one of these heaps.
//  - When a consumer wishes to bind their descriptor to a descriptor table, they should copy the
//    descriptor using the ID3D12Device::CopyDescriptors API over into a heap that is managed as a
//    ring-buffer. The lifetime of this copied descriptor is transient, and will expire at the end
//    of its frame.
//
// Why are we managing descriptors like this?
// Ultimately, we need to make sure that only one descriptor heap is bound to the pipeline for the
// entire frame (in this case it would be the heap managed as a ring buffer). This is necessary for
// performance on some GPUs, as changing the bound heap can be expensive.
// 

struct DescriptorAllocation {
  D3D12_CPU_DESCRIPTOR_HANDLE cpuStart;
  D3D12_GPU_DESCRIPTOR_HANDLE gpuStart;
  size_t numDescriptors;
  size_t indexInHeap;
};

// This class is used to allocate descriptors, well, linearly. The idea is that you allocate all of
// the descriptors in one of these objects. Then, when you need to access it through a shader,
// CopyDescriptorsSimple them over into a CircularBufferDescriptorAllocator.
//
// Eventually we will do this better and maintain a free-list so that the allocator can free up any
// descriptors after their descriptors are no longer needed. But for now, any allocation done is
// permanent.
//
// These are not-shader-visible descriptors.
class LinearDescriptorAllocator {
private:
  ID3D12Device* m_device = nullptr;

  // TODO:
  //   1) Manage multiple heaps to allow for dynamic allocation
  //   2) Maintain free-list of descriptors
  //      2a) We'll therefore have to return some kind of thread-safe handle that can free the
  //          allocation when necessary.
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
  D3D12_CPU_DESCRIPTOR_HANDLE m_heapStart;
  unsigned int m_descriptorIncrementSize;

  size_t m_currentHeapSize;
  size_t m_currentIndex;

public:
  void Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type);

  DescriptorAllocation AllocateSingleDescriptor();
};

class CircularBufferDescriptorAllocator {
 private:
  ID3D12Device* m_device = nullptr;

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
  D3D12_CPU_DESCRIPTOR_HANDLE m_heapStartCPU;
  D3D12_GPU_DESCRIPTOR_HANDLE m_heapStartGPU;
  unsigned int m_descriptorIncrementSize;

  size_t m_heapSize;
  size_t m_firstFreeIndex;
  size_t m_numFreeDescriptors;

  struct InFlightDescriptorRange {
    size_t startIndex;
    size_t numDescriptors;
    size_t signalValue;
  };
  std::optional<InFlightDescriptorRange> m_currentAllocation = std::nullopt;
  std::queue<InFlightDescriptorRange> m_inFlightDescriptorRanges;

 public:
  void Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type);
  ID3D12DescriptorHeap* GetDescriptorHeap();

  // TODO: This also needs to return the GPU descriptor for SetGraphicsRootDescriptorTable.
  DescriptorAllocation AllocateSingleDescriptor(size_t nextSignalValue);
  //DescriptorAllocation AllocateDescriptorRange(size_t nextSignalValue, size_t numDescriptors); // TODO.
  void Cleanup(size_t signalValue);
};
