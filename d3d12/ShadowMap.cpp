#include "d3d12/ShadowMap.h"

#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

void ShadowMap::Initialize(ID3D12Device* device, unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;

  D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC shadowMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_D32_FLOAT, width, height, /*arraySize*/ 1, /*mipLevels*/ 1, /*sampleCount*/ 1,
      /*sampleQuality*/ 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
  D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

  HR(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &shadowMapDesc,
                                     D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue,
                                     IID_PPV_ARGS(&m_depthTarget)));

  D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc;
  dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsvDescriptorHeapDesc.NumDescriptors = 1;
  dsvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dsvDescriptorHeapDesc.NodeMask = 0;
  HR(device->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&m_dsvDescriptorHeap)));

  // According to the documentation: "If the format chosen is DXGI_FORMAT_UNKNOWN, the format of the
  // parent resource is used". So we might want to switch to that eventually. 
  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
  //dsvDesc.Format = DXGI_FORMAT_UNKNOWN;
  dsvDesc.Format = shadowMapDesc.Format;
  dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  dsvDesc.Texture2D.MipSlice = 0;

  CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle(
      m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
  device->CreateDepthStencilView(m_depthTarget.Get(), &dsvDesc, dsvDescriptorHandle);
  m_dsvDescriptorHandle = dsvDescriptorHandle;
}

ID3D12Resource* ShadowMap::GetShadowMap() {
  return m_depthTarget.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE ShadowMap::GetDescriptorHandle() const {
  return m_dsvDescriptorHandle;
}

unsigned int ShadowMap::GetWidth() const {
  return m_width;
}

unsigned int ShadowMap::GetHeight() const {
  return m_height;
}