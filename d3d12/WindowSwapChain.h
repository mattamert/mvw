#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>  // For ComPtr

#define NUM_BACK_BUFFERS 2

class WindowSwapChain {
  Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[NUM_BACK_BUFFERS];
  HANDLE m_frameWaitableObjectHandle;

  HWND m_hwnd;
  unsigned int m_clientWidth;
  unsigned int m_clientHeight;

 public:
  void Initialize(IDXGIFactory2* factory,
                  ID3D12Device* device,
                  ID3D12CommandQueue* commandQueue,
                  HWND hwnd);

  void HandleResize(unsigned int width, unsigned int height);

  void WaitForNextFrame();
  void Present();

  ID3D12Resource* GetCurrentBackBuffer();
  unsigned int GetWidth() const;
  unsigned int GetHeight() const;
  float GetAspectRatio() const;
};
