#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr
#include <cstdint>

namespace ResourceHelper {
Microsoft::WRL::ComPtr<ID3D12Resource> AllocateBuffer(ID3D12Device* device,
                                                      unsigned int bytesToAllocate);
}