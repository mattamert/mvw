#include "Pass.h"

#include <d3dcompiler.h>

#include "d3d12/comhelper.h"
#include "d3d12/d3dx12.h"

using namespace Microsoft::WRL;

namespace {
HRESULT CompileShader(LPCWSTR srcFile, LPCSTR entryPoint, LPCSTR profile, /*out*/ ID3DBlob** blob) {
  if (!srcFile || !entryPoint || !profile || !blob)
    return E_INVALIDARG;

  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
  flags |= D3DCOMPILE_DEBUG;
#endif

  ID3DBlob* shaderBlob = nullptr;
  ID3DBlob* errorBlob = nullptr;
  HRESULT hr = D3DCompileFromFile(srcFile, /*defines*/ nullptr, /*include*/ nullptr, entryPoint,
                                  profile, flags, 0, &shaderBlob, &errorBlob);
  if (FAILED(hr)) {
    if (errorBlob) {
      OutputDebugStringA((char*)errorBlob->GetBufferPointer());
      errorBlob->Release();
    }
    SafeRelease(&shaderBlob);
    return hr;
  }

  *blob = shaderBlob;
  return hr;
}
}  // namespace

void ColorPass::Initialize(ID3D12Device* device) {
#if 1
  const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
      /*shaderRegister*/ 0, /*D3D12_FILTER*/ D3D12_FILTER_ANISOTROPIC,
      D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
      D3D12_TEXTURE_ADDRESS_MODE_WRAP);

  CD3DX12_DESCRIPTOR_RANGE texTable;
  texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

  CD3DX12_ROOT_PARAMETER parameters[3] = {};
  parameters[0].InitAsConstantBufferView(/*shaderRegister*/ 0, /*registerSpace*/ 0,
                                         D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[1].InitAsConstantBufferView(/*shaderRegister*/ 1, /*registerSpace*/ 0,
                                         D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[2].InitAsDescriptorTable(/*numDescriptorRanges*/ 1, /*pDescriptorRanges*/ &texTable,
                                      D3D12_SHADER_VISIBILITY_PIXEL);

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
  rootSignatureDesc.NumParameters = 3;
  rootSignatureDesc.pParameters = parameters;
  rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  rootSignatureDesc.NumStaticSamplers = 1;
  rootSignatureDesc.pStaticSamplers = &pointClamp;

  ComPtr<ID3DBlob> rootSignatureBlob;
  ComPtr<ID3DBlob> errorBlob;
  HR(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
                                 &rootSignatureBlob, &errorBlob));
  HR(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
                                 rootSignatureBlob->GetBufferSize(),
                                 IID_PPV_ARGS(&m_rootSignature)));

  HR(CompileShader(L"SimpleCameraAndPositioning.hlsl", "VSMain", "vs_5_0", &m_vertexShader));
  HR(CompileShader(L"SimpleCameraAndPositioning.hlsl", "PSMain", "ps_5_0", &m_pixelShader));
#else
  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
  rootSignatureDesc.NumParameters = 0;
  rootSignatureDesc.pParameters = nullptr;
  rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  rootSignatureDesc.NumStaticSamplers = 0;
  rootSignatureDesc.pStaticSamplers = nullptr;

  ComPtr<ID3DBlob> rootSignatureBlob;
  ComPtr<ID3DBlob> errorBlob;
  HR(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
                                 &rootSignatureBlob, &errorBlob));
  HR(m_device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
                                   rootSignatureBlob->GetBufferSize(),
                                   IID_PPV_ARGS(&m_rootSignature)));

  HR(CompileShader(L"PassThroughShaders.hlsl", "VSMain", "vs_5_0", &m_vertexShader));
  HR(CompileShader(L"PassThroughShaders.hlsl", "PSMain", "ps_5_0", &m_pixelShader));
#endif

  D3D12_INPUT_ELEMENT_DESC inputElements[] = {
      {"POSITION", /*SemanticIndex*/ 0, DXGI_FORMAT_R32G32B32_FLOAT, /*InputSlot*/ 0,
       /*AlignedByteOffset*/ 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       /*InstanceDataStepRate*/ 0},
      {"TEXCOORD", /*SemanticIndex*/ 0, DXGI_FORMAT_R32G32_FLOAT, /*InputSlot*/ 0,
       /*AlignedByteOffset*/ 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       /*InstanceDataStepRate*/ 0},
      {"NORMAL", /*SemanticIndex*/ 0, DXGI_FORMAT_R32G32B32_FLOAT, /*InputSlot*/ 0,
       /*AlignedByteOffset*/ 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       /*InstanceDataStepRate*/ 0},
  };

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = {inputElements, _countof(inputElements)};
  psoDesc.pRootSignature = m_rootSignature.Get();
  psoDesc.VS = {m_vertexShader->GetBufferPointer(), m_vertexShader->GetBufferSize()};
  psoDesc.PS = {m_pixelShader->GetBufferPointer(), m_pixelShader->GetBufferSize()};
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_NONE;
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  psoDesc.SampleDesc.Count = 1;
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

  HR(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

ID3D12PipelineState* ColorPass::GetPipelineState() {
  return m_pipelineState.Get();
}

ID3D12RootSignature* ColorPass::GetRootSignature() {
  return m_rootSignature.Get();
}
