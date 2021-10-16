#include "d3d12/RenderTarget.h"

#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

void RenderTarget::Initialize(ID3D12Device* device, unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;
  InitializeRenderTargetTexture(device);
  InitializeDepthStencilMembers(device);
}

void RenderTarget::HandleResize(ID3D12Device* device, unsigned int width, unsigned int height) {
  if (m_width != width || m_height != height) {
    m_width = width;
    m_height = height;
    InitializeRenderTargetTexture(device);
    InitializeDepthStencilMembers(device);
  }
}

void RenderTarget::InitializeRenderTargetTexture(ID3D12Device* device) {
  CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC renderTargetResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_R8G8B8A8_UNORM, m_width, m_height, /*arraySize*/ 1, /*mipLevels*/ 1);
  renderTargetResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  //float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
  float clearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
  D3D12_CLEAR_VALUE d3d12ClearValue = CD3DX12_CLEAR_VALUE(renderTargetResourceDesc.Format, clearColor);
  HR(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                     &renderTargetResourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                     &d3d12ClearValue, IID_PPV_ARGS(&m_renderTarget)));

  if (!m_rtvDescriptorHeap) {
    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
    rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDescriptorHeapDesc.NumDescriptors = NUM_RENDER_TARGETS;
    rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvDescriptorHeapDesc.NodeMask = 0;

    HR(device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap)));
  }

  D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc;
  rtvViewDesc.Format = renderTargetResourceDesc.Format;
  rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtvViewDesc.Texture2D.MipSlice = 0;
  rtvViewDesc.Texture2D.PlaneSlice = 0;

  CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHeapStart(
      m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

  CD3DX12_CPU_DESCRIPTOR_HANDLE destination(descriptorHeapStart);
  device->CreateRenderTargetView(m_renderTarget.Get(), &rtvViewDesc, destination);
  m_renderTargetDescriptorHandle = destination;
}

void RenderTarget::InitializeDepthStencilMembers(ID3D12Device* device) {
  CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC depthStencilBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_D32_FLOAT, m_width, m_height, /*arraySize*/ 1, /*mipLevels*/ 1);
  depthStencilBufferDesc.Flags =
      D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
  D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.f, 0);
  HR(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &depthStencilBufferDesc,
                                     D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
                                     IID_PPV_ARGS(&m_depthStencilBuffer)));

  if (!m_dsvDescriptorHeap) {
    D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc;
    dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDescriptorHeapDesc.NumDescriptors = 1;
    dsvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvDescriptorHeapDesc.NodeMask = 0;

    HR(device->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&m_dsvDescriptorHeap)));
  }

  // According to the documentation: "If the format chosen is DXGI_FORMAT_UNKNOWN, the format of the
  // parent resource is used". So we might want to switch to that eventually.
  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
  //dsvDesc.Format = DXGI_FORMAT_UNKNOWN;
  dsvDesc.Format = depthStencilBufferDesc.Format;
  dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  dsvDesc.Texture2D.MipSlice = 0;

  CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle(
      m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
  m_dsvDescriptorHandle = dsvDescriptorHandle;

  device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, dsvDescriptorHandle);
}

ID3D12Resource* RenderTarget::GetRenderTargetResource() {
  return m_renderTarget.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTarget::GetRenderTargetRTVHandle() const {
  return m_renderTargetDescriptorHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTarget::GetDepthStencilViewHandle() const {
  return m_dsvDescriptorHandle;
}
