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

namespace Pass {
Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateTownscaperRootSignature(ID3D12Device* device);
Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateTownscaperPipelineState_Buildings(ID3D12Device* device,
                                                                                    ID3D12RootSignature* rootSignature);
Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateTownscaperPipelineState_Generic(ID3D12Device* device,
                                                                                  ID3D12RootSignature* rootSignature);
}  // namespace Pass
