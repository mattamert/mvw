#pragma once

#include <d3d12.h>
#include <wrl/client.h>  // for ComPtr

#include <queue>

class ResourceGarbageCollector {
  struct Garbage {
    uint64_t signalValue;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  };
  std::queue<Garbage> m_garbage;

 public:
  void MarkAsGarbage(Microsoft::WRL::ComPtr<ID3D12Resource> resource, uint64_t signalValue);
  void Cleanup(uint64_t signalValue);
};
