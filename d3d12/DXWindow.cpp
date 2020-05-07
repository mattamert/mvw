#include "DXWindow.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_4.h>
#include <Windows.h>

#include <string>

#include "comhelper.h"

#define NUM_BACK_BUFFERS 2

using Microsoft::WRL::ComPtr;

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

#define IDT_TIMER1 1234

namespace {
void EnableDebugLayer() {
  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    debugController->EnableDebugLayer();
}

ComPtr<IDXGIAdapter> FindAdapter(IDXGIFactory4* factory) {
  for (UINT adapterIndex = 0;; ++adapterIndex) {
    ComPtr<IDXGIAdapter> adapter;
    if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters(0, &adapter)) {
      break;
    }

    if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                    _uuidof(ID3D12Device), nullptr))) {
      return adapter;
    }
  }

  return nullptr;
}

}  // namespace


void DXWindow::Initialize() {
  this->hwnd = CreateDXWindow(this, L"DXWindow", 640, 480);

  EnableDebugLayer();

  Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
  HR(CreateDXGIFactory(IID_PPV_ARGS(&factory)));

  ComPtr<IDXGIAdapter> adapter = FindAdapter(factory.Get());
  ComPtr<ID3D12Device> device;
  HR(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

  D3D12_COMMAND_QUEUE_DESC queueDesc;
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queueDesc.NodeMask = 0;

  ComPtr<ID3D12CommandQueue> graphicsQueue;
  HR(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&graphicsQueue)));

  ComPtr<ID3D12CommandAllocator> commandAllocator;
  HR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                    IID_PPV_ARGS(&commandAllocator)));

  DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
  swapChainDesc.Width = 0;  // Use automatic sizing.
  swapChainDesc.Height = 0;
  swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // This is the most common swap chain format.
  swapChainDesc.Stereo = false;

  swapChainDesc.SampleDesc.Count = 1;  // Don't use multi-sampling.
  swapChainDesc.SampleDesc.Quality = 0;

  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount = 2;  // Use double-buffering to minimize latency.
  // swapChainDesc.Scaling = DXGI_SCALING_NONE;
  swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
  // swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  // All Windows Store apps must use this SwapEffect.
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;  // Bitblt.
  swapChainDesc.Flags = 0;

  ComPtr<IDXGISwapChain1> swapChain;
  HR(factory->CreateSwapChainForHwnd(device.Get(), this->hwnd, &swapChainDesc,
                                     /*pFullscreenDesc*/ nullptr,
                                     /*pRestrictToOutput*/ nullptr, &swapChain));

  D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeap;
  rtvDescriptorHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvDescriptorHeap.NumDescriptors = NUM_BACK_BUFFERS;
  rtvDescriptorHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  rtvDescriptorHeap.NodeMask = 0;

  ComPtr<ID3D12DescriptorHeap> renderTargetViewDescriptorHeap;
  HR(device->CreateDescriptorHeap(&rtvDescriptorHeap,
                                  IID_PPV_ARGS(&renderTargetViewDescriptorHeap)));

  D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc;
  rtvViewDesc.Format = swapChainDesc.Format;
  rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtvViewDesc.Texture2D.MipSlice = 0;
  rtvViewDesc.Texture2D.PlaneSlice = 0;

  UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for (size_t i = 0; i < NUM_BACK_BUFFERS; ++i)
  {
    ComPtr<ID3D12Resource> backBuffer;
    HR(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

    UINT rtvDescriptorOffset = rtvDescriptorSize * i;
    D3D12_CPU_DESCRIPTOR_HANDLE descriptorPtr;
    descriptorPtr.ptr = size_t(renderTargetViewDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr) + size_t(rtvDescriptorOffset);

    device->CreateRenderTargetView(backBuffer.Get(), &rtvViewDesc, descriptorPtr);
  }

  device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), );
}

void DXWindow::OnResize(unsigned int width, unsigned int height) {}

void DXWindow::DrawScene() {}

void DXWindow::Present() {}


// --------------------------- Window-handling code ---------------------------

static LRESULT CALLBACK DXWindowWndProc(HWND hwnd,
  UINT message,
  WPARAM wParam,
  LPARAM lParam) {
  DXWindow* app =
    reinterpret_cast<DXWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  switch (message) {
  case WM_CREATE: {
    LPCREATESTRUCT createStruct = (LPCREATESTRUCT)lParam;
    DXWindow* app = (DXWindow*)createStruct->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);

    SetTimer(hwnd,              // handle to main window
      IDT_TIMER1,        // timer identifier
      16,                // 10-second interval
      (TIMERPROC)NULL);  // no timer callback

    return 0;
  }

  case WM_TIMER:
    if (wParam == IDT_TIMER1) {
      InvalidateRect(hwnd, nullptr, false);
    }
    return 0;

  case WM_SIZE:
    if (app != nullptr) {
      UINT width = LOWORD(lParam);
      UINT height = HIWORD(lParam);
      app->OnResize(width, height);
    }
    return 0;

  case WM_DISPLAYCHANGE:
    if (app != nullptr) {
      InvalidateRect(hwnd, nullptr, false);
    }
    return 0;

  case WM_PAINT:
    if (app != nullptr) {
      //app->Animate();
      app->DrawScene();
      app->Present();
      ValidateRect(hwnd, nullptr);
    }
    return 0;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;

  default:
    return DefWindowProc(hwnd, message, wParam, lParam);
  }
}

static const wchar_t* g_className = L"DXWindow";

void DXWindow::RegisterDXWindow() {
  WNDCLASSEXW windowClass;  // = { sizeof(WNDCLASSEX) };
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = DXWindowWndProc;
  windowClass.cbClsExtra = 0;
  windowClass.cbWndExtra = sizeof(LONG_PTR);
  windowClass.hInstance = HINST_THISCOMPONENT;
  windowClass.hbrBackground = NULL;
  windowClass.lpszMenuName = NULL;
  windowClass.hCursor = LoadCursor(NULL, IDI_APPLICATION);
  windowClass.lpszClassName = g_className;
  windowClass.hIcon = NULL;
  windowClass.hIconSm = NULL;

  ATOM result = RegisterClassExW(&windowClass);
  DWORD err = GetLastError();

  // This is so that we don't have to necessarily keep track of whether or not
  // we've called this function before.
  // We just assume that this will be the only function in this process that
  // will register a window class with this class name.
  if (result == 0 && err != ERROR_CLASS_ALREADY_EXISTS)
    HR(E_FAIL);
}

HWND DXWindow::CreateDXWindow(DXWindow* window, const std::wstring& windowName, int width, int height) {
  DXWindow::RegisterDXWindow();
  HWND hwnd = CreateWindowW(g_className,           // Class name
                            windowName.c_str(),    // Window Name.
                            WS_OVERLAPPEDWINDOW,   // Style
                            CW_USEDEFAULT,         // x
                            CW_USEDEFAULT,         // y
                            width,                 // Horiz size (pixels)
                            height,                // Vert size (pixels)
                            NULL,                  // parent window
                            NULL,                  // menu
                            HINST_THISCOMPONENT,   // Instance ...?
                            window                 // lparam
  );

  if (hwnd) {
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
  }

  return hwnd;
}

