#pragma once
#include <d3d11_1.h>
#include <d2d1_1.h>

struct Size
{
  unsigned int width;
  unsigned int height;
};

class DirectXBase
{
public:
  DirectXBase();
  ~DirectXBase();

  void Initialize();
  virtual void OnResize(unsigned int width, unsigned int height);
  virtual void DrawScene() = 0;
  void Present();

  virtual void Animate() {}

private:
  void CreateDeviceIndependentResourcesForDXBase();
  void CreateDeviceResourcesForDXBase();
  void CreateWindowSizeDependentResourcesForDXBase();

protected:

  // Helper function for Initialize(). 
  virtual void CreateDeviceIndependentResources() { };

  // Helper functions for Initialization/Reinitialization in OnResize.
  virtual void CreateDeviceResources() { };
  virtual void CreateWindowSizeDependentResources() { };

private:
  // Helper for CreateDeviceResources.
  void CreateSwapChain();

  // Helpers for CreateWindowSizeDependentResources.
  void CreateRenderTargetView();
  void CreateDepthStencilBufferView();
  void SetViewportTransform();

  // Windowing shit.
  void RegisterDXWindow() const;
  HRESULT CreateDXWindow();

  // We allow whoever is deriving from us access to the variables, as that
  // is kind of necessary for DirectX stuff...
protected:

  // ----------------------   Window stuff.   ----------------------

  HWND m_hwnd;
  Size m_windowBounds;
  float m_dpiX, m_dpiY;

  ID2D1Factory1* m_d2dFactory; // Device Independent.

  // ----------------------  Direct3D stuff.  ----------------------

  ID3D11Device1* m_d3dDevice; // Device resource.
  ID3D11DeviceContext1* m_d3dContext; // Device resource.
  ID3D11RenderTargetView* m_d3dRenderTargetView; // Window-size dependent.
  ID3D11DepthStencilView* m_d3dDepthStencilView; // Window-size dependent.

  // ----------------------     DXGI shit.     ----------------------

  IDXGISwapChain1* m_swapChain; // Device-Depenent. (BUT--- it must be resized...)
  D3D_FEATURE_LEVEL m_featureLevel;
};