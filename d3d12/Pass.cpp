#include "d3d12/Pass.h"

#include <d3dcompiler.h>

#include "d3d12/d3dx12.h"
#include "utils/comhelper.h"

using namespace Microsoft::WRL;

ID3D12PipelineState* GraphicsPass::GetPipelineState() {
  return m_pipelineState.Get();
}

ID3D12RootSignature* GraphicsPass::GetRootSignature() {
  return m_rootSignature.Get();
}

namespace {
HRESULT CompileShader(LPCWSTR srcFile,
                      LPCSTR entryPoint,
                      LPCSTR profile,
                      /*out*/ Microsoft::WRL::ComPtr<ID3DBlob>& blob) {
  if (!srcFile || !entryPoint || !profile)
    return E_INVALIDARG;

  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
  flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

  Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob = nullptr;
  Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
  HRESULT hr = D3DCompileFromFile(srcFile, /*defines*/ nullptr, /*include*/ nullptr, entryPoint, profile, flags, 0,
                                  &shaderBlob, &errorBlob);
  if (FAILED(hr)) {
    if (errorBlob) {
      OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    return hr;
  }

  blob = std::move(shaderBlob);
  return hr;
}

ComPtr<ID3D12RootSignature> SerializeAndCreateRootSignature(ID3D12Device* device,
                                                            D3D12_ROOT_SIGNATURE_DESC* rootSignatureDesc) {
  ComPtr<ID3DBlob> rootSignatureBlob;
  ComPtr<ID3DBlob> errorBlob;
  if (FAILED(D3D12SerializeRootSignature(rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob,
                                         &errorBlob))) {
    if (errorBlob) {
      OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    HR(E_FAIL);
  }

  ComPtr<ID3D12RootSignature> rootSignature;
  HR(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(),
                                 IID_PPV_ARGS(&rootSignature)));

  return rootSignature;
}
}  // namespace

void ColorPass::Initialize(ID3D12Device* device) {
  const CD3DX12_STATIC_SAMPLER_DESC staticSamplers[] = {
      CD3DX12_STATIC_SAMPLER_DESC(
          /*shaderRegister*/ 0, /*D3D12_FILTER*/ D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
          D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
      CD3DX12_STATIC_SAMPLER_DESC(
          /*shaderRegister*/ 1, /*D3D12_FILTER*/ D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
          D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
          /*mipLODBias*/ 0.f, /*maxAnisotropy*/ 16, D3D12_COMPARISON_FUNC_LESS_EQUAL)};

  CD3DX12_DESCRIPTOR_RANGE shadowMapTable;
  shadowMapTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

  CD3DX12_DESCRIPTOR_RANGE texTable;
  texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

  // CBV 0 is the per-frame data.
  // CBV 1 is the per-object data.
  CD3DX12_ROOT_PARAMETER parameters[4] = {};
  parameters[0].InitAsConstantBufferView(/*shaderRegister*/ 0, /*registerSpace*/ 0, D3D12_SHADER_VISIBILITY_ALL);
  parameters[1].InitAsConstantBufferView(/*shaderRegister*/ 1, /*registerSpace*/ 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[2].InitAsDescriptorTable(/*numDescriptorRanges*/ 1, /*pDescriptorRanges*/ &shadowMapTable,
                                      D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[3].InitAsDescriptorTable(/*numDescriptorRanges*/ 1, /*pDescriptorRanges*/ &texTable,
                                      D3D12_SHADER_VISIBILITY_PIXEL);

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
  rootSignatureDesc.NumParameters = 4;
  rootSignatureDesc.pParameters = parameters;
  rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  rootSignatureDesc.NumStaticSamplers = 2;
  rootSignatureDesc.pStaticSamplers = staticSamplers;
  m_rootSignature = SerializeAndCreateRootSignature(device, &rootSignatureDesc);

  Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
  Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
  HR(CompileShader(L"ColorPassShaders.hlsl", "VSMain", "vs_5_0", /*out*/ vertexShader));
  HR(CompileShader(L"ColorPassShaders.hlsl", "PSMain", "ps_5_0", /*out*/ pixelShader));

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
  psoDesc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
  psoDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
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

void ShadowMapPass::Initialize(ID3D12Device* device) {
  // This is the sampler for determining if the texture at a certain point is fully transparent;
  // therefore, just use point sampling.
  const CD3DX12_STATIC_SAMPLER_DESC pointSampler(
      /*shaderRegister*/ 0, /*D3D12_FILTER*/ D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
      D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);

  CD3DX12_ROOT_PARAMETER parameters[2] = {};
  parameters[0].InitAsConstantBufferView(/*shaderRegister*/ 0, /*registerSpace*/ 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[1].InitAsConstantBufferView(/*shaderRegister*/ 1, /*registerSpace*/ 0, D3D12_SHADER_VISIBILITY_VERTEX);

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
  rootSignatureDesc.NumParameters = 2;
  rootSignatureDesc.pParameters = parameters;
  rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  rootSignatureDesc.NumStaticSamplers = 1;
  rootSignatureDesc.pStaticSamplers = &pointSampler;
  m_rootSignature = SerializeAndCreateRootSignature(device, &rootSignatureDesc);

  Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
  Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
  HR(CompileShader(L"ShadowMapShaders.hlsl", "VSMain", "vs_5_0", /*out*/ vertexShader));
  HR(CompileShader(L"ShadowMapShaders.hlsl", "PSMain", "ps_5_0", /*out*/ pixelShader));

  D3D12_INPUT_ELEMENT_DESC inputElements[] = {
      {"POSITION", /*SemanticIndex*/ 0, DXGI_FORMAT_R32G32B32_FLOAT, /*InputSlot*/ 0,
       /*AlignedByteOffset*/ 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       /*InstanceDataStepRate*/ 0},
      {"TEXCOORD", /*SemanticIndex*/ 0, DXGI_FORMAT_R32G32_FLOAT, /*InputSlot*/ 0,
       /*AlignedByteOffset*/ 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       /*InstanceDataStepRate*/ 0},
  };

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = {inputElements, _countof(inputElements)};
  psoDesc.pRootSignature = m_rootSignature.Get();
  psoDesc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
  psoDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_NONE;
  psoDesc.RasterizerState.DepthBias = 50000;
  psoDesc.RasterizerState.DepthBiasClamp = 0.f;
  psoDesc.RasterizerState.SlopeScaledDepthBias = 1.f;
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 0;  // Only render depths for the shadow map.
  psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
  psoDesc.SampleDesc.Count = 1;
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

  HR(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

namespace {
ComPtr<ID3D12RootSignature> CreateTownscaperRootSignature(ID3D12Device* device) {
  const CD3DX12_STATIC_SAMPLER_DESC staticSamplers[] = {
      CD3DX12_STATIC_SAMPLER_DESC(
          /*shaderRegister*/ 0, /*D3D12_FILTER*/ D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
          D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
      CD3DX12_STATIC_SAMPLER_DESC(
          /*shaderRegister*/ 1, /*D3D12_FILTER*/ D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
          D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
          /*mipLODBias*/ 0.f, /*maxAnisotropy*/ 1, D3D12_COMPARISON_FUNC_LESS_EQUAL)};

  CD3DX12_DESCRIPTOR_RANGE shadowMapTable;
  shadowMapTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

  CD3DX12_DESCRIPTOR_RANGE texTable;
  texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

  // CBV 0 is the per-frame data.
  // CBV 1 is the per-object data.
  CD3DX12_ROOT_PARAMETER parameters[4] = {};
  parameters[0].InitAsConstantBufferView(/*shaderRegister*/ 0, /*registerSpace*/ 0, D3D12_SHADER_VISIBILITY_ALL);
  parameters[1].InitAsConstantBufferView(/*shaderRegister*/ 1, /*registerSpace*/ 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[2].InitAsDescriptorTable(/*numDescriptorRanges*/ 1, /*pDescriptorRanges*/ &shadowMapTable,
                                      D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[3].InitAsDescriptorTable(/*numDescriptorRanges*/ 1, /*pDescriptorRanges*/ &texTable,
                                      D3D12_SHADER_VISIBILITY_PIXEL);

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
  rootSignatureDesc.NumParameters = 4;
  rootSignatureDesc.pParameters = parameters;
  rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  rootSignatureDesc.NumStaticSamplers = 2;
  rootSignatureDesc.pStaticSamplers = staticSamplers;
  return SerializeAndCreateRootSignature(device, &rootSignatureDesc);
}

ComPtr<ID3D12RootSignature> CreateTownscaperShadowMapRootSignature(ID3D12Device* device) {
  const CD3DX12_STATIC_SAMPLER_DESC staticSamplers[] = {CD3DX12_STATIC_SAMPLER_DESC(
      /*shaderRegister*/ 0, /*D3D12_FILTER*/ D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
      D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP)};

  CD3DX12_DESCRIPTOR_RANGE texTable;
  texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

  // CBV 0 is the per-frame data.
  // CBV 1 is the per-object data.
  CD3DX12_ROOT_PARAMETER parameters[3] = {};
  parameters[0].InitAsConstantBufferView(/*shaderRegister*/ 0, /*registerSpace*/ 0, D3D12_SHADER_VISIBILITY_ALL);
  parameters[1].InitAsConstantBufferView(/*shaderRegister*/ 1, /*registerSpace*/ 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[2].InitAsDescriptorTable(/*numDescriptorRanges*/ 1, /*pDescriptorRanges*/ &texTable,
                                      D3D12_SHADER_VISIBILITY_PIXEL);

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
  rootSignatureDesc.NumParameters = 3;
  rootSignatureDesc.pParameters = parameters;
  rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  rootSignatureDesc.NumStaticSamplers = 1;
  rootSignatureDesc.pStaticSamplers = staticSamplers;
  return SerializeAndCreateRootSignature(device, &rootSignatureDesc);
}
}  // namespace

void TownscaperPSOs::Initialize(ID3D12Device* device) {
  m_rootSignature = CreateTownscaperRootSignature(device);
  m_shadowMapPassRootSignature = CreateTownscaperShadowMapRootSignature(device);

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


  Microsoft::WRL::ComPtr<ID3DBlob> genericVS;
  Microsoft::WRL::ComPtr<ID3DBlob> buildingsPS;
  Microsoft::WRL::ComPtr<ID3DBlob> emptyPS;
  Microsoft::WRL::ComPtr<ID3DBlob> genericColorPS;
  Microsoft::WRL::ComPtr<ID3DBlob> shadowMapVS;
  Microsoft::WRL::ComPtr<ID3DBlob> shadowMapPS;
  HR(CompileShader(L"Townscaper.hlsl", "VSMain", "vs_5_0", /*out*/ genericVS));
  HR(CompileShader(L"Townscaper.hlsl", "PSMain_Empty", "ps_5_0", /*out*/ emptyPS));
  HR(CompileShader(L"Townscaper.hlsl", "PSMain_Buildings", "ps_5_0", /*out*/ buildingsPS));
  HR(CompileShader(L"Townscaper.hlsl", "PSMain_NonBuildings", "ps_5_0", /*out*/ genericColorPS));
  HR(CompileShader(L"Townscaper_ShadowMap.hlsl", "VSMain", "vs_5_0", /*out*/ shadowMapVS));
  HR(CompileShader(L"Townscaper_ShadowMap.hlsl", "PSMain", "ps_5_0", /*out*/ shadowMapPS));

  D3D12_GRAPHICS_PIPELINE_STATE_DESC basePSO = {};
  basePSO.InputLayout = {inputElements, _countof(inputElements)};
  basePSO.pRootSignature = m_rootSignature.Get();
  basePSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  basePSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  basePSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
  basePSO.SampleMask = UINT_MAX;
  basePSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  basePSO.NumRenderTargets = 1;
  basePSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  basePSO.SampleDesc.Count = 1;
  basePSO.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

  const int shadowMapDepthBias = 50000;
  const float shadowMapDepthBiasClamp = 0.f;
  const float shadowMapSlopeScaledDepthBias = 1.f;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC baseShadowMapPSO = {};
  baseShadowMapPSO.InputLayout = {inputElements, 2};  // Purposefully ignore the normal - shadow maps don't need it.
  baseShadowMapPSO.pRootSignature = m_shadowMapPassRootSignature.Get();
  baseShadowMapPSO.VS = { shadowMapVS->GetBufferPointer(), shadowMapVS->GetBufferSize() };
  baseShadowMapPSO.PS = { shadowMapPS->GetBufferPointer(), shadowMapPS->GetBufferSize() };
  baseShadowMapPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  baseShadowMapPSO.RasterizerState.DepthBias = shadowMapDepthBias;
  baseShadowMapPSO.RasterizerState.DepthBiasClamp = shadowMapDepthBiasClamp;
  baseShadowMapPSO.RasterizerState.SlopeScaledDepthBias = shadowMapSlopeScaledDepthBias;
  baseShadowMapPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  baseShadowMapPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
  baseShadowMapPSO.SampleMask = UINT_MAX;
  baseShadowMapPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  baseShadowMapPSO.NumRenderTargets = 0;  // Only render depths for the shadow map.
  baseShadowMapPSO.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
  baseShadowMapPSO.SampleDesc.Count = 1;
  baseShadowMapPSO.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC buildingsPSO = basePSO;
    buildingsPSO.VS = {genericVS->GetBufferPointer(), genericVS->GetBufferSize()};
    buildingsPSO.PS = {buildingsPS->GetBufferPointer(), buildingsPS->GetBufferSize()};
    HR(device->CreateGraphicsPipelineState(&buildingsPSO, IID_PPV_ARGS(&m_psoBuildings)));
  }

  {
    D3D12_DEPTH_STENCIL_DESC windowsStencilDepthStencilState;
    windowsStencilDepthStencilState.DepthEnable = TRUE;
    windowsStencilDepthStencilState.DepthEnable = TRUE;
    windowsStencilDepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    windowsStencilDepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    windowsStencilDepthStencilState.StencilEnable = TRUE;
    windowsStencilDepthStencilState.StencilReadMask = 0x1;
    windowsStencilDepthStencilState.StencilWriteMask = 0x1;
    windowsStencilDepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    windowsStencilDepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    windowsStencilDepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
    windowsStencilDepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    windowsStencilDepthStencilState.BackFace = windowsStencilDepthStencilState.FrontFace;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC windowsStencilPSO = basePSO;
    windowsStencilPSO.VS = {genericVS->GetBufferPointer(), genericVS->GetBufferSize()};
    windowsStencilPSO.PS = {emptyPS->GetBufferPointer(), emptyPS->GetBufferSize()};
    windowsStencilPSO.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_NONE;
    windowsStencilPSO.DepthStencilState = windowsStencilDepthStencilState;
    windowsStencilPSO.NumRenderTargets = 0;
    windowsStencilPSO.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    HR(device->CreateGraphicsPipelineState(&windowsStencilPSO, IID_PPV_ARGS(&m_psoWindows_Stencil)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC windowsStencilPSO_ShadowMap = baseShadowMapPSO;
    windowsStencilPSO_ShadowMap.DepthStencilState = windowsStencilDepthStencilState;
    windowsStencilPSO_ShadowMap.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_NONE;
    HR(device->CreateGraphicsPipelineState(&windowsStencilPSO_ShadowMap,
                                           IID_PPV_ARGS(&m_psoShadowMap_Windows_Stencil)));
  }

  {
    D3D12_DEPTH_STENCIL_DESC windowsMaxDepthStencilState;
    windowsMaxDepthStencilState.DepthEnable = TRUE;
    windowsMaxDepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    windowsMaxDepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    windowsMaxDepthStencilState.StencilEnable = TRUE;
    windowsMaxDepthStencilState.StencilReadMask = 0x1;
    windowsMaxDepthStencilState.StencilWriteMask = 0x1;
    windowsMaxDepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    windowsMaxDepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    windowsMaxDepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    windowsMaxDepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
    windowsMaxDepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    windowsMaxDepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    windowsMaxDepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    windowsMaxDepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC windowsMaxDepthPSO = basePSO;
    windowsMaxDepthPSO.VS = {genericVS->GetBufferPointer(), genericVS->GetBufferSize()};
    windowsMaxDepthPSO.PS = {emptyPS->GetBufferPointer(), emptyPS->GetBufferSize()};
    windowsMaxDepthPSO.DepthStencilState = windowsMaxDepthStencilState;
    windowsMaxDepthPSO.NumRenderTargets = 0;
    windowsMaxDepthPSO.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    HR(device->CreateGraphicsPipelineState(&windowsMaxDepthPSO, IID_PPV_ARGS(&m_psoWindows_MaxDepth)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC windowsMapDepthPSO_ShadowMap = baseShadowMapPSO;
    windowsMapDepthPSO_ShadowMap.DepthStencilState = windowsMaxDepthStencilState;
    HR(device->CreateGraphicsPipelineState(&windowsMapDepthPSO_ShadowMap, IID_PPV_ARGS(&m_psoShadowMap_Windows_MaxDepth)));
  }

  {
    D3D12_DEPTH_STENCIL_DESC windowsMinDepthStencilState;
    windowsMinDepthStencilState.DepthEnable = TRUE;
    windowsMinDepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    windowsMinDepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    windowsMinDepthStencilState.StencilEnable = TRUE;
    windowsMinDepthStencilState.StencilReadMask = 0x1;
    windowsMinDepthStencilState.StencilWriteMask = 0x1;
    windowsMinDepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    windowsMinDepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    windowsMinDepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    windowsMinDepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
    windowsMinDepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    windowsMinDepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    windowsMinDepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    windowsMinDepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC windowsColorPSO = basePSO;
    windowsColorPSO.VS = {genericVS->GetBufferPointer(), genericVS->GetBufferSize()};
    windowsColorPSO.PS = {genericColorPS->GetBufferPointer(), genericColorPS->GetBufferSize()};
    windowsColorPSO.DepthStencilState = windowsMinDepthStencilState;
    HR(device->CreateGraphicsPipelineState(&windowsColorPSO, IID_PPV_ARGS(&m_psoWindows_MinDepth_Color)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC windowsMinDepth_ShadowMap = baseShadowMapPSO;
    windowsMinDepth_ShadowMap.DepthStencilState = windowsMinDepthStencilState;
    HR(device->CreateGraphicsPipelineState(&windowsMinDepth_ShadowMap, IID_PPV_ARGS(&m_psoShadowMap_Windows_MinDepth)));
  }

  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC genericColorPSO = basePSO;
    genericColorPSO.VS = {genericVS->GetBufferPointer(), genericVS->GetBufferSize()};
    genericColorPSO.PS = {genericColorPS->GetBufferPointer(), genericColorPS->GetBufferSize()};
    HR(device->CreateGraphicsPipelineState(&genericColorPSO, IID_PPV_ARGS(&m_psoGenericColor)));
  }

  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowMapPSO = baseShadowMapPSO;
    HR(device->CreateGraphicsPipelineState(&shadowMapPSO, IID_PPV_ARGS(&m_psoShadowMap_Generic)));
  }
}
