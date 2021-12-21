#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

class GraphicsPass {
 protected:
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

 public:
  ID3D12PipelineState* GetPipelineState();
  ID3D12RootSignature* GetRootSignature();

  virtual void Initialize(ID3D12Device* device) = 0;
};

class ColorPass : public GraphicsPass {
 public:
  struct PerFrameData {
    DirectX::XMFLOAT4X4 projectionViewTransform;
    DirectX::XMFLOAT4X4 shadowMapProjectionViewTransform;
    DirectX::XMFLOAT4 lightDirection;
  };

  struct PerObjectData {
    DirectX::XMFLOAT4X4 modelTransform;
    DirectX::XMFLOAT4X4 modelTransformInverseTranspose;
  };

  void Initialize(ID3D12Device* device) override;
};

class ShadowMapPass : public GraphicsPass {
 public:
  struct PerFrameData {
    DirectX::XMFLOAT4X4 projectionViewTransform;
  };

  struct PerObjectData {
    DirectX::XMFLOAT4X4 worldTransform;
  };

  void Initialize(ID3D12Device* device) override;
};

struct TownscaperPSOs {
  Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoBuildings;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoWindows_Stencil;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoWindows_MaxDepth;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoWindows_MinDepth;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoWindows_MinDepth_Color;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoGenericColor;

  void Initialize(ID3D12Device* device);
};
