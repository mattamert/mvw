#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>  // For ComPtr

#include <string>

#define NUM_BACK_BUFFERS 2

class DXWindow {
 private:
  bool m_isInitialized;
  HWND m_hwnd;

  // Per-device data.
  // TODO: These should really be abstracted into their own class, as they are not exactly bound to
  // one window.
  Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
  Microsoft::WRL::ComPtr<ID3D12Device> m_device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_directCommandQueue;
  // Command allocators and command lists especially should be split into something different, as
  // there should be 1 per render thread.
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_directCommandAllocators[NUM_BACK_BUFFERS];
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cl;

  // Per-window data.
  Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
  UINT m_currentBackBufferIndex;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[NUM_BACK_BUFFERS];

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_backBufferDescriptorHandles[NUM_BACK_BUFFERS];
  Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;  // Is this actually per-window?
  HANDLE m_fenceEvent;
  uint64_t m_fenceValues[NUM_BACK_BUFFERS];
  uint64_t m_nextFenceValue = 1;  // This must be initialized to 1, since the fences start out at 0.

  unsigned int m_clientWidth;
  unsigned int m_clientHeight;

  // Per-pass data.
  // TODO: Probably want a way to share pipeline states between passes?
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
  Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader;
  Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader;

  // App data.
  Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

 public:
  DXWindow();
  void Initialize();
  bool IsInitialized() const;

  void OnResize(unsigned int width, unsigned int height);
  void DrawScene();
  void PresentAndSignal();

 private:
  // Note: InitializePerDeviceObjects must be called before the other two.
  void InitializePerDeviceObjects();
  void InitializePerWindowObjects();
  void InitializePerPassObjects();
  void InitializeAppObjects();

  void FlushGPUWork();
  void WaitForNextFrame();

  // Window-handling functions.
  static void RegisterDXWindow();
  static HWND CreateDXWindow(DXWindow* window,
                             const std::wstring& windowName,
                             int width,
                             int height);
  static void ShowAndUpdateDXWindow(HWND hwnd);
};
