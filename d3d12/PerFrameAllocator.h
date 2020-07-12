#pragma once

#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

#include <queue>
#include <vector>

class PerFrameAllocator {
  struct FrameAllocation {
    uint64_t signalValue;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> resources;
  };
  std::queue<FrameAllocation> m_allocations;

 public:
  Microsoft::WRL::ComPtr<ID3D12Resource> AllocateBuffer(ID3D12Device* device,
                                                        uint64_t frameSignalValue,
                                                        unsigned int bytesToAllocate);
  void Cleanup(uint64_t frameSignalValue);
};