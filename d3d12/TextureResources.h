#pragma once

#include "d3d12/DescriptorHeapManagers.h"

#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

class TextureResource {
 protected:
  Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
  unsigned int m_width;
  unsigned int m_height;

 public:
  ID3D12Resource* GetResource();
  unsigned int GetWidth() const;
  unsigned int GetHeight() const;
};

class RenderTargetTexture : public TextureResource {
  D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle;

 public:
  void Initialize(ID3D12Device* device,
                  D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorDestination,
                  unsigned int width,
                  unsigned int height);
  void Resize(ID3D12Device* device, unsigned int width, unsigned int height);

  D3D12_CPU_DESCRIPTOR_HANDLE GetRTVDescriptorHandle() const;
};

class DepthBufferTexture : public TextureResource {
  DescriptorAllocation m_dsvDescriptor;
  DescriptorAllocation m_srvDescriptor;

 public:
  void Initialize(ID3D12Device* device,
                  const DescriptorAllocation& dsvDescriptorDestination,
                  unsigned int width,
                  unsigned int height);
  void InitializeWithSRV(ID3D12Device* device,
                         const DescriptorAllocation& dsvDescriptorDestination,
                         const DescriptorAllocation& srvDescriptorDestination,
                         unsigned int width,
                         unsigned int height);

  void Resize(ID3D12Device* device, unsigned int width, unsigned int height);

  D3D12_CPU_DESCRIPTOR_HANDLE GetDSVDescriptorHandle() const;
  D3D12_CPU_DESCRIPTOR_HANDLE GetSRVDescriptorHandle() const;
};
