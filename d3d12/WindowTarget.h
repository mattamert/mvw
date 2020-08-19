#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>  // For ComPtr

#define NUM_BACK_BUFFERS 2

class WindowTarget {
  Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[NUM_BACK_BUFFERS];
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_backBufferDescriptorHandles[NUM_BACK_BUFFERS];
  HANDLE m_frameWaitableObjectHandle;

  // Since we are using a waitable swap chain, we only really need one depth/stencil buffer.
  Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvDescriptorHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_dsvDescriptorHandle;

  HWND m_hwnd;
  unsigned int m_clientWidth;
  unsigned int m_clientHeight;

  unsigned int m_pendingClientWidth;
  unsigned int m_pendingClientHeight;
  bool m_isResizePending = false;

  void InitializeDepthStemcilMembers(ID3D12Device* device, unsigned int width, unsigned int height);

 public:
  void Initialize(IDXGIFactory2* factory,
                  ID3D12Device* device,
                  ID3D12CommandQueue* commandQueue,
                  HWND hwnd);

  bool HasPendingResize() const;
  void AddPendingResize(unsigned int width, unsigned int height);
  void HandlePendingResize();

  void WaitForNextFrame();
  void Present();

  ID3D12Resource* GetCurrentBackBuffer();
  D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRTVHandle();
  D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilViewHandle();

  unsigned int GetWidth() const;
  unsigned int GetHeight() const;
  float GetAspectRatio() const;
};