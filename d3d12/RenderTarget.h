#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

#define NUM_RENDER_TARGETS 1

class RenderTarget {
  Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTarget;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_renderTargetDescriptorHandle;

  // Since we are using a waitable swap chain, we only really need one depth/stencil buffer.
  Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvDescriptorHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_dsvDescriptorHandle;

  unsigned int m_width;
  unsigned int m_height;

  void InitializeRenderTargetTexture(ID3D12Device* device);
  void InitializeDepthStencilMembers(ID3D12Device* device);
 public:
  void Initialize(ID3D12Device* device, unsigned int width, unsigned int height);
  void HandleResize(ID3D12Device* device, unsigned int width, unsigned int height);

  ID3D12Resource* GetRenderTargetResource();

  D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetRTVHandle() const;
  D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilViewHandle() const;
};