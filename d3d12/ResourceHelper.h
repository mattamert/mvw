#pragma once

#include "d3d12/ResourceGarbageCollector.h"

#include <cstdint>

#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

namespace ResourceHelper {
Microsoft::WRL::ComPtr<ID3D12Resource> AllocateBuffer(ID3D12Device* device,
                                                      unsigned int bytesToAllocate);

Microsoft::WRL::ComPtr<ID3D12Resource> AllocateIntermediateBuffer(
    ID3D12Device* device,
    ID3D12Resource* destinationResource,
    ResourceGarbageCollector& garbageCollector,
    uint64_t nextSignalValue);

void UpdateBuffer(ID3D12Resource* buffer, void* newData, size_t dataSize);
}  // namespace ResourceHelper