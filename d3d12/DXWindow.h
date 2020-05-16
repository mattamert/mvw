#include <d3d12.h>
#include <dxgi1_4.h>
#include <Windows.h>
#include <wrl/client.h> // For ComPtr

#include <string>

class DXWindow {
private:
  bool isInitialized;
  HWND hwnd;

  // Per-device data.
  // TODO: These should really be abstracted into their own class, as they are not exactly bound to one window.
  Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
  Microsoft::WRL::ComPtr<ID3D12Device> device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> directCommandQueue;
  // * Command allocators and command lists especially should be split into something different, as there should be 1 per render thread.
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> directCommandAllocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cl;

  // Per-window data.
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;

  // Per-pass data.
  // TODO: Probably want a way to share pipeline states between passes?
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
  Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
  Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;

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
  static HWND CreateDXWindow(DXWindow* window, const std::wstring& windowName, int width, int height);
};
