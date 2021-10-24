#include "d3d12/TextureResources.h"

#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

ID3D12Resource* TextureResource::GetResource() {
  return m_resource.Get();
}

unsigned int TextureResource::GetWidth() const {
  return m_width;
}

unsigned int TextureResource::GetHeight() const {
  return m_height;
}

void RenderTargetTexture::Initialize(ID3D12Device* device,
                                     D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorDestination,
                                     unsigned int width,
                                     unsigned int height) {
  m_width = width;
  m_height = height;
  m_rtvHandle = rtvDescriptorDestination;

  CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC renderTargetResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_R8G8B8A8_UNORM, m_width, m_height, /*arraySize*/ 1, /*mipLevels*/ 1);
  renderTargetResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  // float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
  float clearColor[4] = {0.1f, 0.2f, 0.3f, 1.0f};
  D3D12_CLEAR_VALUE d3d12ClearValue =
      CD3DX12_CLEAR_VALUE(renderTargetResourceDesc.Format, clearColor);
  HR(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                     &renderTargetResourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                     &d3d12ClearValue, IID_PPV_ARGS(&m_resource)));

  D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc;
  rtvViewDesc.Format = renderTargetResourceDesc.Format;
  rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtvViewDesc.Texture2D.MipSlice = 0;
  rtvViewDesc.Texture2D.PlaneSlice = 0;

  device->CreateRenderTargetView(m_resource.Get(), &rtvViewDesc, rtvDescriptorDestination);
}

void RenderTargetTexture::Resize(ID3D12Device* device, unsigned int width, unsigned int height) {
  if (m_width != width || m_height != height) {
    Initialize(device, m_rtvHandle, width, height);
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetTexture::GetRTVDescriptorHandle() const {
  return m_rtvHandle;
}

void DepthBufferTexture::Initialize(ID3D12Device* device,
                                    const DescriptorAllocation& dsvDescriptorDestination,
                                    const DescriptorAllocation& srvDescriptorDestination,
                                    unsigned int width,
                                    unsigned int height) {
  m_dsvDescriptor = dsvDescriptorDestination;
  m_srvDescriptor = srvDescriptorDestination;
  m_width = width;
  m_height = height;

  D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  // TODO: Right now we're creating all depth buffers as R32_TYPELESS; however, this is only
  // necessary when we're creating a depth buffer that also needs to be used as an SRV. We should
  // optimize the path where we don't need to use the depth buffer as an SRV.
  D3D12_RESOURCE_DESC depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_R32_TYPELESS, width, height, /*arraySize*/ 1, /*mipLevels*/ 1, /*sampleCount*/ 1,
      /*sampleQuality*/ 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
  D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);
  HR(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &depthBufferDesc,
                                     D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
                                     IID_PPV_ARGS(&m_resource)));

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
  dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
  dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  dsvDesc.Texture2D.MipSlice = 0;
  device->CreateDepthStencilView(m_resource.Get(), &dsvDesc, dsvDescriptorDestination.cpuStart);

  // Create SRV.
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = 1;
  srvDesc.Texture2D.MostDetailedMip = 0;
  srvDesc.Texture2D.PlaneSlice = 0;
  srvDesc.Texture2D.ResourceMinLODClamp = 0;
  device->CreateShaderResourceView(m_resource.Get(), &srvDesc, srvDescriptorDestination.cpuStart);
}

void DepthBufferTexture::Resize(ID3D12Device* device, unsigned int width, unsigned int height) {
  if (m_width != width || m_height != height) {
    Initialize(device, m_dsvDescriptor, m_srvDescriptor, width, height);
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthBufferTexture::GetDSVDescriptorHandle() const {
  return m_dsvDescriptor.cpuStart;
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthBufferTexture::GetSRVDescriptorHandle() const {
  return m_srvDescriptor.cpuStart;
}
