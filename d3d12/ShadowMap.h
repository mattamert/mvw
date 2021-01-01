#pragma once

#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

class ShadowMap {
  Microsoft::WRL::ComPtr<ID3D12Resource> m_depthTarget; // DXGI_FORMAT_D32_FLOAT
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvDescriptorHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_dsvDescriptorHandle;
  unsigned int m_width = 0;
  unsigned int m_height = 0;

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvDescriptorHeap;
  D3D12_GPU_DESCRIPTOR_HANDLE m_srvDescriptorHandle;

public:
  void Initialize(ID3D12Device* device, unsigned int width, unsigned int height);
  ID3D12Resource* GetShadowMap();
  D3D12_CPU_DESCRIPTOR_HANDLE GetDSVDescriptorHandle() const;
  unsigned int GetWidth() const;
  unsigned int GetHeight() const;

  ID3D12DescriptorHeap* GetSRVHeap();
  D3D12_GPU_DESCRIPTOR_HANDLE GetSRVDescriptorHandle() const;
};
