#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

#include <vector>

class ConstantBufferAllocator {
 private:
  ID3D12Device* m_device;

  Microsoft::WRL::ComPtr<ID3D12Resource> m_currentBuffer;
  size_t m_currentBufferOffset = 0;
  uint8_t* m_currentMappedAddress = nullptr;
  uint64_t m_currentBufferSignalValue = 0;

  struct InFlightPage {
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    size_t signalValue;
  };
  std::vector<InFlightPage> m_inFlightPages;

  // TODO: Instead of just deallocating pages that are now no longer InFlight, we could put them in
  // a free list and use them the next time we need to allocate a page. That would save us from
  // having to allocate them again.

 public:
  void Initialize(ID3D12Device* device);
  D3D12_GPU_VIRTUAL_ADDRESS AllocateAndUpload(size_t dataSizeInBytes,
                                              void* data,
                                              uint64_t nextSignalValue);
  void Cleanup(uint64_t currentSignalValue);

  // TODO: It would be useful to have an EndFrame function to let the constant buffer allocator know
  // that the current buffer is safe to move to InFlight.
};