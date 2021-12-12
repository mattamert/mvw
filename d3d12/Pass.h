#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

// TODO: Maybe want to have a base class that these classes inherit from?
class ColorPass {
  // TODO: Probably want a way to share pipeline states between passes?
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
  Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader;
  Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader;

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

  void Initialize(ID3D12Device* device);

  ID3D12PipelineState* GetPipelineState();
  ID3D12RootSignature* GetRootSignature();
};

class ShadowMapPass {
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
  Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader;
  Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader;

 public:
  struct PerFrameData {
    DirectX::XMFLOAT4X4 projectionViewTransform;
  };

  struct PerObjectData {
    DirectX::XMFLOAT4X4 worldTransform;
  };

  void Initialize(ID3D12Device* device);

  ID3D12PipelineState* GetPipelineState();
  ID3D12RootSignature* GetRootSignature();
};
