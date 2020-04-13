#include <Windows.h>

#undef min
#undef max 

#include <algorithm>
#include <assert.h>

#include "DirectXBase.h"
#include "Helper.h"



#define NUM_BACK_BUFFERS 2

// -------------------------------  Useful shit  -------------------------------

#if defined(_DEBUG)
// Check for SDK Layer support.
inline bool SdkLayersAvailable()
{
  HRESULT hr = D3D11CreateDevice(
    nullptr,
    D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
    0,
    D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
    nullptr,                    // Any feature level will do.
    0,
    D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
    nullptr,                    // No need to keep the D3D device reference.
    nullptr,                    // No need to know the feature level.
    nullptr                     // No need to keep the D3D device context reference.
  );

  return SUCCEEDED(hr);
}
#endif

// -------------------------------  Member function shit  -------------------------------

DirectXBase::DirectXBase() :
  m_hwnd(NULL),
  m_d2dFactory(nullptr),

  m_d3dDevice(nullptr),
  m_d3dContext(nullptr),
  m_d3dRenderTargetView(nullptr),
  m_d3dDepthStencilView(nullptr),

  m_swapChain(nullptr)
{
}

DirectXBase::~DirectXBase()
{
  SafeRelease(&m_d2dFactory);

  SafeRelease(&m_d3dDevice);
  SafeRelease(&m_d3dContext);
  SafeRelease(&m_d3dRenderTargetView);
  SafeRelease(&m_d3dDepthStencilView);

  SafeRelease(&m_swapChain);
}

void DirectXBase::Initialize()
{
  DirectXBase::RegisterDXWindow();

  CreateDeviceIndependentResourcesForDXBase();
  this->CreateDeviceIndependentResources();

  // Note: Window creation should only be done after creating the device.
  // This is so that we can query the d2d device for the desktop DPI.
  // @Cleanup: Should probably handle window creation failure in a more elegant way.
  HR(this->CreateDXWindow());

  // The winproc will be called several times in the above call to create the window.
  // The WM_SIZE message will call the OnResize function, which is set up to initialize
  // the rest of the members. We therefore can stop initializing here.

  // This is actually very important because after that first WM_SIZE message, a
  // WM_PAINT message is then sent, and is not sent again unless resized.
  // Therefore, if we do the simple thing and just ignore the messages until we're entirely 
  // initialized via this function, then  we end up with a blank window until we resize the
  // window (which triggers another WM_PAINT message).

  // @TODO: We should probably not necessarily rely on WM_PAINT message to do all
  // of the rendering, especially if we want animations.
  // We may still be able to use this, however, if we have a Timer that will send the
  // app a WM_PAINT message when we need to animate something...
}

void DirectXBase::Present()
{
  assert(m_swapChain);

  m_swapChain->Present(1, 0);
}

void DirectXBase::CreateDeviceIndependentResourcesForDXBase()
{
  // Create D2D stuff here.
  D2D1_FACTORY_OPTIONS options;
  ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
  // If the project is in a debug build, enable Direct2D debugging via SDK Layers.
  options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

  HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, (void**)&m_d2dFactory));

  // Initialize other D2D or DWrite stuff below...
}

// These are the resources that depend on the device.
void DirectXBase::CreateDeviceResourcesForDXBase()
{
  // This flag adds support for surfaces with a different color channel ordering
  // than the API default. It is required for compatibility with Direct2D.
  UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
  if (SdkLayersAvailable())
  {
    // If the project is in a debug build, enable debugging via SDK Layers with this flag.
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
  }
#endif

  // This array defines the set of DirectX hardware feature levels this app will support.
  // Note the ordering should be preserved.
  // Don't forget to declare your application's minimum required feature level in its
  // description.  All applications are assumed to support 9.1 unless otherwise stated.
  D3D_FEATURE_LEVEL featureLevels[] =
  {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      //d3d_feature_level_10_1,
      //d3d_feature_level_10_0,
      //d3d_feature_level_9_3,
      //d3d_feature_level_9_2,
      //d3d_feature_level_9_1
  };

  // Create the Direct3D 11 API device object and a corresponding context.
  ID3D11Device* device;
  ID3D11DeviceContext* context;
  HRESULT hr = D3D11CreateDevice(
    nullptr,                    // Specify nullptr to use the default adapter.
    D3D_DRIVER_TYPE_HARDWARE,
    0,
    creationFlags,              // Set debug and Direct2D compatibility flags.
    featureLevels,              // List of feature levels this app can support.
    ARRAYSIZE(featureLevels),
    D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
    &device,                    // Returns the Direct3D device created.
    &m_featureLevel,            // Returns feature level of device created.
    &context                    // Returns the device immediate context.
  );

  assert(SUCCEEDED(hr));

  // Get the Direct3D 11.1 API device and context interfaces.
  HR(device->QueryInterface(IID_PPV_ARGS(&m_d3dDevice)));
  HR(context->QueryInterface(IID_PPV_ARGS(&m_d3dContext)));
  SafeRelease(&device);
  SafeRelease(&context);

  // Get the underlying DXGI device of the Direct3D device.
  IDXGIDevice* dxgiDevice;
  HR(m_d3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice)));

  SafeRelease(&dxgiDevice);

  // Create the DXGI swap chain.
  CreateSwapChain();
}

void DirectXBase::CreateWindowSizeDependentResourcesForDXBase()
{
  CreateRenderTargetView();
  CreateDepthStencilBufferView();
  SetViewportTransform(); // The transform should match the window size, so we set it here.
}

void DirectXBase::OnResize(unsigned int width, unsigned int height)
{
  if (!m_d3dDevice)
  {
    // This happens when the window is first created, and the winproc is calling us. The device-dependent
    // members are completely uninitialized. Therefore, all we have to do is create them for the first time.

    m_windowBounds.width = width;
    m_windowBounds.height = height;

    CreateDeviceResourcesForDXBase();
    CreateDeviceResources();
    CreateWindowSizeDependentResourcesForDXBase();
    CreateWindowSizeDependentResources();
  }
  else if (m_windowBounds.width != width || m_windowBounds.height != height)
  {
    m_windowBounds.width = width;
    m_windowBounds.height = height;

    SafeRelease(&m_d3dRenderTargetView);
    SafeRelease(&m_d3dDepthStencilView);

    assert(m_swapChain != nullptr);
    HRESULT hr = m_swapChain->ResizeBuffers(NUM_BACK_BUFFERS, 0, 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED)
    {
      // If the device was removed for any reason, a new device and swapchain will need to be created.
      // @TODO: This can be handled very elegantly. For now, we just assume this will never happen.
      //        However, we do want to bring special attention to it in case it does happen (you are, after all,
      //        writing this on the atrocity that is the Surface Book).
      assert(false);

      // The original code would have re-set up the shit and everything, so it was done here.
      // I'm keeeping the return statement here just as a reminder to do that...
      return;
    }
    else
    {
      HR(hr);
    }

    CreateWindowSizeDependentResourcesForDXBase();
    CreateWindowSizeDependentResources();
  }
}

void DirectXBase::CreateSwapChain()
{
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
  swapChainDesc.Width = 0;                                     // Use automatic sizing.
  swapChainDesc.Height = 0;
  swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;           // This is the most common swap chain format.
  swapChainDesc.Stereo = false;

  swapChainDesc.SampleDesc.Count = 1;                          // Don't use multi-sampling.
  swapChainDesc.SampleDesc.Quality = 0;

  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount = NUM_BACK_BUFFERS;                               // Use double-buffering to minimize latency.
  //swapChainDesc.Scaling = DXGI_SCALING_NONE;
  swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
  //swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // All Windows Store apps must use this SwapEffect.
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL; // Bitblt.
  swapChainDesc.Flags = 0;

  IDXGIDevice1* dxgiDevice;
  HR(m_d3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice)));

  IDXGIAdapter* dxgiAdapter;
  HR(dxgiDevice->GetAdapter(&dxgiAdapter));

  IDXGIFactory2* dxgiFactory;
  HR(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));

  HRESULT hr = dxgiFactory->CreateSwapChainForHwnd(
    m_d3dDevice,
    this->m_hwnd,
    &swapChainDesc,
    nullptr,
    nullptr,
    &m_swapChain
  );

  assert(SUCCEEDED(hr));

  // Ensure that DXGI does not queue more than one frame at a time. This both reduces latency and
  // ensures that the application will only render after each VSync, minimizing power consumption.
  // TODO: IDK if we need this?? (Or what it ever does...)
  HR(dxgiDevice->SetMaximumFrameLatency(1));

  SafeRelease(&dxgiDevice);
  SafeRelease(&dxgiAdapter);
  SafeRelease(&dxgiFactory);
}

void DirectXBase::CreateRenderTargetView()
{
  // Create a Direct3D render target view of the swap chain back buffer.
  ID3D11Texture2D* backBuffer;
  HR(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));

  HR(
    m_d3dDevice->CreateRenderTargetView(
      backBuffer,
      nullptr, // Render Target View Description -- we don't need to do anything special with it.
      &m_d3dRenderTargetView // Output.
    )
  );

  SafeRelease(&backBuffer);
}

void DirectXBase::CreateDepthStencilBufferView()
{
  // Create a depth stencil view for use with 3D rendering if needed.
  D3D11_TEXTURE2D_DESC depthStencilDesc;
  depthStencilDesc.Width = std::max(this->m_windowBounds.width, 1u);
  depthStencilDesc.Height = this->m_windowBounds.height;
  depthStencilDesc.MipLevels = 1;
  depthStencilDesc.ArraySize = 1;
  depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

  depthStencilDesc.SampleDesc.Count = 1;
  depthStencilDesc.SampleDesc.Quality = 0;

  depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
  depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  depthStencilDesc.CPUAccessFlags = 0;
  depthStencilDesc.MiscFlags = 0;

  ID3D11Texture2D* depthStencil;
  HR(m_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, &depthStencil));

  // @TODO: IDK what this even is....
  //auto viewDesc = CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D);
  HR(m_d3dDevice->CreateDepthStencilView(depthStencil, nullptr, &m_d3dDepthStencilView));

  // @IDK: IDK why we don't need to keep the DepthStencil texture...
  SafeRelease(&depthStencil);
}

// IDK what this does tbh.
void DirectXBase::SetViewportTransform()
{
  // Set the 3D rendering viewport to target the entire window.
  D3D11_VIEWPORT screenViewport;
  screenViewport.TopLeftX = 0;
  screenViewport.TopLeftY = 0;
  screenViewport.Width = static_cast<float>(this->m_windowBounds.width);
  screenViewport.Height = static_cast<float>(this->m_windowBounds.height);
  screenViewport.MinDepth = 0.0f;
  screenViewport.MaxDepth = 1.0f;

  m_d3dContext->RSSetViewports(1, &screenViewport);
}

// -------------------------------  Window shit  -------------------------------

// @Cleanup: Should probably move this to a separate header.
// And probably all of the window management stuff...
#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

// Forward declaration for WndProc, as we define that in the main.cpp file for now.
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void DirectXBase::RegisterDXWindow() const
{
  WNDCLASSEX windowClass;// = { sizeof(WNDCLASSEX) };
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = WndProc;
  windowClass.cbClsExtra = 0;
  windowClass.cbWndExtra = sizeof(LONG_PTR);
  windowClass.hInstance = HINST_THISCOMPONENT;
  windowClass.hbrBackground = NULL;
  windowClass.lpszMenuName = NULL;
  windowClass.hCursor = LoadCursor(NULL, IDI_APPLICATION);
  windowClass.lpszClassName = L"D2DDemoApp";
  windowClass.hIcon = NULL;
  windowClass.hIconSm = NULL;

  ATOM result = RegisterClassEx(&windowClass);
  DWORD err = GetLastError();

  // This is so that we don't have to necessarily keep track of whether or not
  // we've called this function before.
  // We just assume that this will be the only function in this process that will register
  // a window class with this class name.
  if (result == 0 && err != ERROR_CLASS_ALREADY_EXISTS)
    throw;
}

HRESULT DirectXBase::CreateDXWindow()
{
  assert(m_d2dFactory != nullptr);

  // Because the CreateWindow function takes its size in pixels,
  // obtain the system DPI and use it to scale the window size.

  // The factory returns the current system DPI. This is also the value
  // it will use to create its own windows.
  m_d2dFactory->GetDesktopDpi(&m_dpiX, &m_dpiY);

  // Create the window.
  m_hwnd = CreateWindow(
    L"D2DDemoApp", // Class name
    L"Direct2D Demo App", // Window Name.
    WS_OVERLAPPEDWINDOW, // Style
    CW_USEDEFAULT, // x
    CW_USEDEFAULT, // y
    static_cast<UINT>(std::ceil(640.f * m_dpiX / 96.f)), // Horiz size
    static_cast<UINT>(std::ceil(480.f * m_dpiY / 96.f)), // Vert size
    NULL, // parent window
    NULL, // menu
    HINST_THISCOMPONENT, // Instance ...?
    this // lparam
  );

  this->m_windowBounds.width = static_cast<UINT>(std::ceil(640.f * m_dpiX / 96.f));
  this->m_windowBounds.height = static_cast<UINT>(std::ceil(480.f * m_dpiY / 96.f));

  HRESULT hr = m_hwnd ? S_OK : E_FAIL;
  if (SUCCEEDED(hr))
  {
    ShowWindow(m_hwnd, SW_SHOWNORMAL);
    UpdateWindow(m_hwnd);
  }

  return hr;
}