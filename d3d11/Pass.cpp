#include "Pass.h"

#include <d3dcompiler.h>

#include <assert.h>
#include "Helper.h"

Pass::~Pass() noexcept {
  SafeRelease(&input_layout_);
  SafeRelease(&vertex_shader_);
  SafeRelease(&pixel_shader_);

  for (size_t i = 0; i < constant_buffers_.size(); ++i) {
    SafeRelease(&constant_buffers_[i]);
  }
}

HRESULT Pass::AddConstantBuffer(ID3D11Device* device, void* initial_data, unsigned int buffer_size_bytes) {
  // Note: constant buffer size must be specified in multiples of 16.
  D3D11_BUFFER_DESC vbd;
  vbd.ByteWidth = buffer_size_bytes;
  vbd.Usage = D3D11_USAGE_DEFAULT;
  vbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  vbd.CPUAccessFlags = 0; // I believe we only need D3D11_CPU_ACCESS_WRITE when we have D3D11_USAGE_DYNAMIC.
  vbd.MiscFlags = 0; // See D3D11_RESOURCE_MISC_FLAG for more details.
  vbd.StructureByteStride = 0; // Only applies to structured buffers.

  D3D11_SUBRESOURCE_DATA subresource_data;
  subresource_data.pSysMem = initial_data;
  subresource_data.SysMemPitch = 0; // (Not used for vertex buffers)
  subresource_data.SysMemSlicePitch = 0; // (Not used for vertex buffers)

  ID3D11Buffer* constant_buffer;
  HRESULT hr = device->CreateBuffer(&vbd, &subresource_data, &constant_buffer);
  if (SUCCEEDED(hr))
    this->constant_buffers_.emplace_back(constant_buffer);

  return hr;
}

void Pass::UpdateConstantBuffer(size_t index, ID3D11DeviceContext* context, void* new_data) {
  assert(this->constant_buffers_.size() > index);
  context->UpdateSubresource(constant_buffers_[index], /*DstSubresource*/0, /*pDstBox*/nullptr, new_data, /*srcRowPitch*/0, /*srcDepthPitch*/0);
}

void Pass::InitializeShadersAndInputLayout(ID3D11Device* device, LPCWSTR vertex_shader_filename, LPCWSTR pixel_shader_filename, D3D11_INPUT_ELEMENT_DESC* vertex_desc) {
  // Currently assumes that entry point is VSMain and PSMain respectively.
  ID3DBlob* vertex_shader_bytecode;
  HR(CompileShader(vertex_shader_filename, "VSMain", "vs_5_0", &vertex_shader_bytecode));
  HR(device->CreateVertexShader(vertex_shader_bytecode->GetBufferPointer(), vertex_shader_bytecode->GetBufferSize(), /*class linkage*/nullptr, &vertex_shader_));

  ID3DBlob* pixel_shader_bytecode;
  HR(CompileShader(pixel_shader_filename, "PSMain", "ps_5_0", &pixel_shader_bytecode));
  HR(device->CreatePixelShader(pixel_shader_bytecode->GetBufferPointer(), pixel_shader_bytecode->GetBufferSize(), nullptr, &pixel_shader_));

  HR(device->CreateInputLayout(vertex_desc, /*NumElements*/2, vertex_shader_bytecode->GetBufferPointer(), vertex_shader_bytecode->GetBufferSize(), &input_layout_));

  SafeRelease(&vertex_shader_bytecode);
  SafeRelease(&pixel_shader_bytecode);
}

HRESULT Pass::CompileShader(LPCWSTR srcFile, LPCSTR entryPoint, LPCSTR profile, /*out*/ID3DBlob** blob)
{
  if (!srcFile || !entryPoint || !profile || !blob)
    return E_INVALIDARG;

  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
  flags |= D3DCOMPILE_DEBUG;
#endif

  ID3DBlob* shaderBlob = nullptr;
  ID3DBlob* errorBlob = nullptr;
  HRESULT hr = D3DCompileFromFile(srcFile, /*defines*/nullptr, /*include*/nullptr, entryPoint, profile, flags, 0, &shaderBlob, &errorBlob);
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

// ------------------------------------------------------------------------------------------------

D3D11_INPUT_ELEMENT_DESC vertex_desc[] = {
  {"POSITION", // SemanticName
   0, // SemanticPosition
   DXGI_FORMAT_R32G32B32_FLOAT, // Format
   0, // InputSlot
   0, // AlignedByteOffset
   D3D11_INPUT_PER_VERTEX_DATA, // InputSlotClass (Instancing things)
   0 // InstanceDataStepRate (Instancing things)
  },
  {"COLOR", // SemanticName
   0, // SemanticPosition
   DXGI_FORMAT_R32G32B32_FLOAT, // Format
   0, // InputSlot
   12, // AlignedByteOffset
   D3D11_INPUT_PER_VERTEX_DATA, // InputSlotClass (Instancing things)
   0 // InstanceDataStepRate (Instancing things)
  },
};

DefaultPass::~DefaultPass() noexcept { }

HRESULT DefaultPass::Initialize(ID3D11Device* device) {
  this->InitializeShadersAndInputLayout(
    device,
    L"VertexShader.hlsl",
    L"PixelShader.hlsl",
    vertex_desc);

  // First constant buffer is the model-view-perspective transform.
  DirectX::XMFLOAT4X4 m;
  DirectX::XMStoreFloat4x4(&m, DirectX::XMMatrixIdentity());
  unsigned int matrix_size = sizeof(DirectX::XMFLOAT4X4);

  // First constant buffer is the view-perspective transform.
  HRESULT hr = this->AddConstantBuffer(device, &m, matrix_size);
  if (!SUCCEEDED(hr))
    return hr;

  // Second constant buffer is the world transform.
  this->AddConstantBuffer(device, &m, matrix_size);
  return S_OK;
}

void DefaultPass::UpdateViewPerspectiveTransform(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* new_transform) {
  this->UpdateConstantBuffer(0, context, new_transform);
}

void DefaultPass::UpdateWorldTransform(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* new_transform) {
  this->UpdateConstantBuffer(1, context, new_transform);
}

void DefaultPass::Draw(ID3D11DeviceContext1* context, const Object* obj) {
  // Make sure we're rendering triangle lists.
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Set up input layout & vertex buffer.
  context->IASetInputLayout(input_layout_);
  UINT stride = sizeof(VertexData);
  UINT offset = 0;
  context->IASetVertexBuffers(0, 1, &obj->mesh.vertex_buffer_, &stride, &offset);
  context->IASetIndexBuffer(obj->mesh.index_buffer_, DXGI_FORMAT_R32_UINT, 0);

  // Set up vertex shader and correlating constant buffers.
  context->VSSetShader(vertex_shader_, nullptr, 0);
  context->VSSetConstantBuffers(0, 1, &constant_buffers_[0]);
  context->VSSetConstantBuffers(1, 1, &constant_buffers_[1]);

  // Set up pixel shader.
  context->PSSetShader(pixel_shader_, nullptr, 0);

  // Draw!
  context->DrawIndexed(obj->mesh.num_indices, 0, 0);
}

