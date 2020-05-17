#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>  // For ComPtr

#include <string>

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
  // * Command allocators and command lists especially should be split into something different, as
  // there should be 1 per render thread.
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_directCommandAllocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cl;

  // Per-window data.
  Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
  Microsoft::WRL::ComPtr<ID3D12Fence>
      m_fence;  // Is this actually per-window? // Should maybe also be 1 per back buffer?

  // Per-pass data.
  // TODO: Probably want a way to share pipeline states between passes?
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
  Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader;
  Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader;

  // Note: InitializePerDeviceObjects must be called before the other two.
  void InitializePerDeviceObjects();
  void InitializePerWindowObjects();
  void InitializePerPassObjects();

 public:
  void Initialize();

  void OnResize(unsigned int width, unsigned int height);
  void DrawScene();
  void Present();

 private:
  static void RegisterDXWindow();
  static HWND CreateDXWindow(DXWindow* window,
                             const std::wstring& windowName,
                             int width,
                             int height);
};
