#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>  // For ComPtr

#include "d3d12/Camera.h"
#include "d3d12/ConstantBufferAllocator.h"
#include "d3d12/DescriptorHeapManagers.h"
#include "d3d12/Pass.h"
#include "d3d12/ResourceGarbageCollector.h"
#include "d3d12/Scene.h"
#include "d3d12/TextureResources.h"
#include "d3d12/WindowSwapChain.h"

class D3D12Renderer {
  // Per-device data.
  Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
  Microsoft::WRL::ComPtr<ID3D12Device> m_device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_directCommandQueue;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_directCommandAllocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cl;

  // Descriptor allocators.
  ConstantBufferAllocator m_constantBufferAllocator;
  LinearDescriptorAllocator m_linearSRVDescriptorAllocator;
  LinearDescriptorAllocator m_linearDSVDescriptorAllocator;
  LinearDescriptorAllocator m_linearRTVDescriptorAllocator;
  CircularBufferDescriptorAllocator m_circularSRVDescriptorAllocator;

  // Window-size dependent resources.
  WindowSwapChain m_window;
  RenderTargetTexture m_renderTarget;
  DepthBufferTexture m_depthBuffer;
  
  // Shadow Maps.
  DepthBufferTexture m_shadowMap;

  // Pass data.
  ColorPass m_colorPass;
  ShadowMapPass m_shadowMapPass;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> m_townscaperRootSignature;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_townscaperPSO_Buildings;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_townscaperPSO_Generic;

  // Fence stuff.
  Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
  HANDLE m_fenceEvent = NULL;
  uint64_t m_nextFenceValue = 1;  // This must be initialized to 1, since fences start out at 0.
  ResourceGarbageCollector m_garbageCollector;

  // Note: InitializePerDeviceObjects must be called before the others.
  void InitializePerDeviceObjects();
  void InitializePerWindowObjects(HWND hwnd);
  void InitializePerPassObjects();
  void InitializeFenceObjects();
  void InitializeShadowMapObjects();

  void Townscaper_RunColorPass(const PinholeCamera& camera,
                                 const OrthographicCamera& shadowMapCamera,
                                 const Object& object);

  void RunShadowPass(const OrthographicCamera& shadowCamera, const Object& object);
  void RunColorPass(const PinholeCamera& camera, const OrthographicCamera& shadowCamera, const Object& object);

public:
  void Initialize(HWND hwnd);
  void HandleResize(unsigned int width, unsigned int height);

  void DrawScene(Scene& scene);
  void WaitForNextFrame();
  void SignalAndPresent();
  void FlushGPUWork();

  // Used for scene initialization.
  // TODO: This is all still pretty sloppy. Need to clean it up somehow.
  Microsoft::WRL::ComPtr<ID3D12Resource> AllocateAndUploadBufferData(const void* data, size_t sizeInBytes);
  Microsoft::WRL::ComPtr<ID3D12Resource> AllocateAndUploadTextureData(const void* textureData,
                                                                      DXGI_FORMAT format,
                                                                      size_t bytesPerPixel,
                                                                      size_t width,
                                                                      size_t height,
                                                                      /*out*/ DescriptorAllocation* srvDescriptor);
  void ExecuteBarriers(size_t numBarriers, const D3D12_RESOURCE_BARRIER* barriers);
  void BeginResourceUpload();
  void FinalizeResourceUpload();
};
