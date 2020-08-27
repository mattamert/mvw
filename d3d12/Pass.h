#pragma once

#include <d3d12.h>
#include <wrl/client.h>  // For ComPtr

class ColorPass {
  // TODO: Probably want a way to share pipeline states between passes?
  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
  Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader;
  Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader;

 public:
  struct VertexData {
    float position[3];
    float textureCoord[2];
    float color[3];
  };

  void Initialize(ID3D12Device* device);

  ID3D12PipelineState* GetPipelineState();
  ID3D12RootSignature* GetRootSignature();
};
