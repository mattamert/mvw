#include "DXApp.h"

#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>

#include <cassert>
#include <string>
#include <iostream>

#include "comhelper.h"
#include "d3dx12.h"

using Microsoft::WRL::ComPtr;

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

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

    if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device),
                                    nullptr))) {
      return adapter;
    }
  }

  return nullptr;
}

HRESULT CompileShader(LPCWSTR srcFile, LPCSTR entryPoint, LPCSTR profile, /*out*/ ID3DBlob** blob) {
  if (!srcFile || !entryPoint || !profile || !blob)
    return E_INVALIDARG;

  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
  flags |= D3DCOMPILE_DEBUG;
#endif

  ID3DBlob* shaderBlob = nullptr;
  ID3DBlob* errorBlob = nullptr;
  HRESULT hr = D3DCompileFromFile(srcFile, /*defines*/ nullptr, /*include*/ nullptr, entryPoint,
                                  profile, flags, 0, &shaderBlob, &errorBlob);
  if (FAILED(hr)) {
    if (errorBlob) {
      OutputDebugStringA((char*)errorBlob->GetBufferPointer());
      errorBlob->Release();
    }
    SafeRelease(&shaderBlob);
    return hr;
  }

  *blob = shaderBlob;
  return hr;
}

}  // namespace

DXWindow::DXWindow()
    : m_isInitialized(false), m_currentBackBufferIndex(0), m_fenceEvent(NULL) {
  for (int i = 0; i < NUM_BACK_BUFFERS; ++i)
    m_fenceValues[i] = 0;
}

void DXWindow::Initialize() {
  EnableDebugLayer();
  m_hwnd = CreateDXWindow(this, L"DXWindow", 640, 480);

  RECT clientArea;
  GetClientRect(m_hwnd, &clientArea);
  m_clientWidth = static_cast<unsigned int>(clientArea.right);
  m_clientHeight = static_cast<unsigned int>(clientArea.bottom);

  InitializePerDeviceObjects();
  InitializePerWindowObjects();
  InitializePerPassObjects();
  InitializeAppObjects();

  FlushGPUWork();
  
  m_isInitialized = true;

  ShowAndUpdateDXWindow(m_hwnd);
}

bool DXWindow::IsInitialized() const {
  return m_isInitialized;
}

void DXWindow::InitializePerDeviceObjects() {
  HR(CreateDXGIFactory(IID_PPV_ARGS(&m_factory)));

  ComPtr<IDXGIAdapter> adapter = FindAdapter(m_factory.Get());
  HR(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

  D3D12_COMMAND_QUEUE_DESC queueDesc;
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queueDesc.NodeMask = 0;
  HR(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_directCommandQueue)));

  for (int i = 0; i < NUM_BACK_BUFFERS; ++i) {
    HR(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        IID_PPV_ARGS(&m_directCommandAllocators[i])));
  }

  HR(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                 m_directCommandAllocators[m_currentBackBufferIndex].Get(),
                                 /*pInitialState*/ nullptr, IID_PPV_ARGS(&m_cl)));
  HR(m_cl->Close());
}

void DXWindow::InitializePerWindowObjects() {
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
  swapChainDesc.Width = 0;  // Use automatic sizing.
  swapChainDesc.Height = 0;
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // This is the most common swap chain format.
  swapChainDesc.Stereo = false;
  swapChainDesc.SampleDesc.Count = 1;  // Don't use multi-sampling.
  swapChainDesc.SampleDesc.Quality = 0;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount = NUM_BACK_BUFFERS;  // Use double-buffering to minimize latency.
  // swapChainDesc.Scaling = DXGI_SCALING_NONE;
  swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
  // Note: All Windows Store apps must use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL.
  // TODO: Maybe use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL? But only if we need to reuse pixels from
  // the previous frame.
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // Bitblt.
  swapChainDesc.Flags = 0;

  ComPtr<IDXGISwapChain1> swapChain;
  HR(m_factory->CreateSwapChainForHwnd(m_directCommandQueue.Get(), m_hwnd, &swapChainDesc,
                                       /*pFullscreenDesc*/ nullptr,
                                       /*pRestrictToOutput*/ nullptr, &swapChain));

  HR(swapChain.As(&m_swapChain));

  D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
  rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvDescriptorHeapDesc.NumDescriptors = NUM_BACK_BUFFERS;
  rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  rtvDescriptorHeapDesc.NodeMask = 0;

  HR(m_device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap)));

  D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc;
  rtvViewDesc.Format = swapChainDesc.Format;
  rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtvViewDesc.Texture2D.MipSlice = 0;
  rtvViewDesc.Texture2D.PlaneSlice = 0;

  CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHeapStart(
      m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
  UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for (size_t i = 0; i < NUM_BACK_BUFFERS; ++i) {
    HR(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));

    UINT rtvDescriptorOffset = descriptorSize * i;
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorPtr(descriptorHeapStart, rtvDescriptorOffset);

    m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvViewDesc, descriptorPtr);
    m_backBufferDescriptorHandles[i] = descriptorPtr;
  }

  HR(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
  m_fenceEvent = CreateEvent(nullptr, /*bManualRestart*/ FALSE, /*bInitialState*/ FALSE, nullptr);
  if (m_fenceEvent == nullptr)
    HR(HRESULT_FROM_WIN32(GetLastError()));
}

void DXWindow::InitializePerPassObjects() {
  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
  rootSignatureDesc.NumParameters = 0;
  rootSignatureDesc.pParameters = nullptr;
  rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  rootSignatureDesc.NumStaticSamplers = 0;
  rootSignatureDesc.pStaticSamplers = nullptr;

  ComPtr<ID3DBlob> rootSignatureBlob;
  ComPtr<ID3DBlob> errorBlob;
  HR(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
                                 &rootSignatureBlob, &errorBlob));
  HR(m_device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
                                   rootSignatureBlob->GetBufferSize(),
                                   IID_PPV_ARGS(&m_rootSignature)));

  HR(CompileShader(L"PassThroughShaders.hlsl", "VSMain", "vs_5_0", &m_vertexShader));
  HR(CompileShader(L"PassThroughShaders.hlsl", "PSMain", "ps_5_0", &m_pixelShader));

  D3D12_INPUT_ELEMENT_DESC inputElements[] = {
      {"POSITION", /*SemanticIndex*/ 0, DXGI_FORMAT_R32G32B32_FLOAT, /*InputSlot*/ 0,
       /*AlignedByteOffset*/ 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       /*InstanceDataStepRate*/ 0},
      {"COLOR", /*SemanticIndex*/ 0, DXGI_FORMAT_R32G32B32_FLOAT, /*InputSlot*/ 0,
       /*AlignedByteOffset*/ 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       /*InstanceDataStepRate*/ 0},
  };

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = {inputElements, _countof(inputElements)};
  psoDesc.pRootSignature = m_rootSignature.Get();
  psoDesc.VS = {m_vertexShader->GetBufferPointer(), m_vertexShader->GetBufferSize()};
  psoDesc.PS = {m_pixelShader->GetBufferPointer(), m_pixelShader->GetBufferSize()};
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthEnable = FALSE;
  psoDesc.DepthStencilState.StencilEnable = FALSE;
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  psoDesc.SampleDesc.Count = 1;

  HR(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

struct VertexData {
  float position[3];
  float color[3];
};

void DXWindow::InitializeAppObjects() {
  // What we need: 1. Actual vertex data. 2. Buffer for the vertex data.
  VertexData vertices[3] = {
      {0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f},
      {0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f},
      {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f},
  };

  const unsigned int bufferSize = sizeof(vertices);

  CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
  HR(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                       IID_PPV_ARGS(&m_vertexBuffer)));

  uint8_t* mappedRegion;
  CD3DX12_RANGE readRange(0, 0);
  HR(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedRegion)));
  memcpy(mappedRegion, vertices, bufferSize);
  m_vertexBuffer->Unmap(0, nullptr);

  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes = bufferSize;
  m_vertexBufferView.StrideInBytes = sizeof(VertexData);
}

void DXWindow::OnResize(unsigned int clientWidth, unsigned int clientHeight) {
  if (m_clientWidth != clientWidth || m_clientHeight != clientHeight) {
    FlushGPUWork();

    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;

    for (int i = 0; i < NUM_BACK_BUFFERS; ++i) {
      m_backBuffers[i].Reset();
    }

    HR(m_swapChain->ResizeBuffers(0, m_clientWidth, m_clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    HR(m_swapChain->GetDesc1(&swapChainDesc));

    D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc;
    rtvViewDesc.Format = swapChainDesc.Format;
    rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvViewDesc.Texture2D.MipSlice = 0;
    rtvViewDesc.Texture2D.PlaneSlice = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHeapStart(
        m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    UINT descriptorSize =
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (size_t i = 0; i < NUM_BACK_BUFFERS; ++i) {
      HR(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));

      UINT rtvDescriptorOffset = descriptorSize * i;
      CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorPtr(descriptorHeapStart, rtvDescriptorOffset);

      m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvViewDesc, descriptorPtr);
      m_backBufferDescriptorHandles[i] = descriptorPtr;
    }

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
  }
}

void DXWindow::DrawScene() {
  WaitForNextFrame();

  ID3D12Resource* backBuffer = m_backBuffers[m_currentBackBufferIndex].Get();
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_backBufferDescriptorHandles[m_currentBackBufferIndex];

  HR(m_directCommandAllocators[m_currentBackBufferIndex]->Reset());
  HR(m_cl->Reset(m_directCommandAllocators[m_currentBackBufferIndex].Get(), m_pipelineState.Get()));

  m_cl->SetGraphicsRootSignature(m_rootSignature.Get());

  CD3DX12_VIEWPORT clientAreaViewport(0.0f, 0.0f, static_cast<float>(m_clientWidth),
                                      static_cast<float>(m_clientHeight));
  CD3DX12_RECT scissorRect(0, 0, m_clientWidth, m_clientHeight);
  m_cl->RSSetViewports(1, &clientAreaViewport);
  m_cl->RSSetScissorRects(1, &scissorRect);

  CD3DX12_RESOURCE_BARRIER rtvResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  m_cl->ResourceBarrier(1, &rtvResourceBarrier);
  m_cl->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

  float clearColor[4] = {0.1, 0.2, 0.3, 1.0};
  m_cl->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  m_cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_cl->IASetVertexBuffers(0, 1, &m_vertexBufferView);
  m_cl->DrawInstanced(3, 1, 0, 0);

  rtvResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  m_cl->ResourceBarrier(1, &rtvResourceBarrier);

  m_cl->Close();

  ID3D12CommandList* cl[] = {m_cl.Get()};
  m_directCommandQueue->ExecuteCommandLists(1, cl);
}

void DXWindow::PresentAndSignal() {
  m_swapChain->Present(1, 0);

  m_fenceValues[m_currentBackBufferIndex] = m_nextFenceValue;
  HR(m_directCommandQueue->Signal(m_fence.Get(), m_nextFenceValue));
  ++m_nextFenceValue;

  m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DXWindow::WaitForNextFrame() {
  if (m_fence->GetCompletedValue() < m_fenceValues[m_currentBackBufferIndex]) {
    HR(m_fence->SetEventOnCompletion(m_fenceValues[m_currentBackBufferIndex], m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
  }
}

void DXWindow::FlushGPUWork() {
  UINT64 fenceValue = m_nextFenceValue;
  HR(m_directCommandQueue->Signal(m_fence.Get(), fenceValue));
  ++m_nextFenceValue;

  if (m_fence->GetCompletedValue() < fenceValue) {
    HR(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
  }

  m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
}

// --------------------------- Window-handling code ---------------------------

static LRESULT CALLBACK DXWindowWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  DXWindow* app = reinterpret_cast<DXWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  switch (message) {
    case WM_CREATE: {
      LPCREATESTRUCT createStruct = (LPCREATESTRUCT)lParam;
      DXWindow* app = (DXWindow*)createStruct->lpCreateParams;
      SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);
      return 0;
    }

    case WM_SIZE:
      if (app != nullptr) {
        assert(app->IsInitialized());
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
      ValidateRect(hwnd, nullptr);
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

HWND DXWindow::CreateDXWindow(DXWindow* window,
                              const std::wstring& windowName,
                              int width,
                              int height) {
  DXWindow::RegisterDXWindow();

  // TODO: Look into how to remove the title bar.
  long windowStyle = WS_OVERLAPPEDWINDOW;
  HWND hwnd = CreateWindowW(g_className,          // Class name
                            windowName.c_str(),   // Window Name.
                            windowStyle,          // Style
                            CW_USEDEFAULT,        // x
                            CW_USEDEFAULT,        // y
                            width,                // Horiz size (pixels)
                            height,               // Vert size (pixels)
                            NULL,                 // parent window
                            NULL,                 // menu
                            HINST_THISCOMPONENT,  // Instance ...?
                            window                // lparam
  );

  return hwnd;
}

void DXWindow::ShowAndUpdateDXWindow(HWND hwnd) {
  if (hwnd) {
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
  }
}
