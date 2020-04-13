#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include <vector>

#include "Object.h"

class Pass {
public:
  virtual ~Pass() noexcept;

  std::vector<ID3D11Buffer*> constant_buffers_;

  ID3D11InputLayout* input_layout_ = nullptr;
  ID3D11VertexShader* vertex_shader_ = nullptr;
  ID3D11PixelShader* pixel_shader_ = nullptr;

protected:
  HRESULT AddConstantBuffer(ID3D11Device* device, void* initial_data, unsigned int buffer_size_bytes);
  void UpdateConstantBuffer(size_t index, ID3D11DeviceContext* context, void* new_data);

  void InitializeShadersAndInputLayout(ID3D11Device* device, LPCWSTR vertex_shader_filename, LPCWSTR pixel_shader_filename, D3D11_INPUT_ELEMENT_DESC* vertex_desc);

  static HRESULT CompileShader(LPCWSTR srcFile, LPCSTR entryPoint, LPCSTR profile, /*out*/ID3DBlob** blob);
};

class DefaultPass : public Pass {
public:
  ~DefaultPass() noexcept;

  struct VertexData {
    float pos_[3];
    float color_[3];
  };

  HRESULT Initialize(ID3D11Device* device);
  void UpdateViewPerspectiveTransform(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* new_transform);
  void UpdateWorldTransform(ID3D11DeviceContext* context, DirectX::XMFLOAT4X4* new_transform);

  void Draw(ID3D11DeviceContext1* device, const Object* obj);
};
