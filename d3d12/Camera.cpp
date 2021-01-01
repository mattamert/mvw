#include "d3d12/Camera.h"

DirectX::XMMATRIX PinholeCamera::GenerateViewPerspectiveTransform(float aspectRatio) const {
  DirectX::XMVECTOR pos = DirectX::XMLoadFloat4(&this->position_);
  DirectX::XMVECTOR look_at = DirectX::XMLoadFloat4(&this->look_at_);
  DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);

  DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookAtLH(pos, look_at, up);
  DirectX::XMMATRIX perspectiveMatrix =
      DirectX::XMMatrixPerspectiveFovLH(0.25f * DirectX::XM_PI, aspectRatio, 0.1f, 1000.f);

  // Future note: DirectXMath uses row-vector matrices and row-major order for the matrices.
  // This means that matrix multplication with the DirectXMath Library should be done as
  // pre-multiplication. As in, the vector being modified should be on the left. e.g.
  //     position[1x4] * model[4x4] * view[4x4] * perspective[4x4]
  // However, the two cancel out when serializing it and they can be treated as column-vector
  // matrices in HLSL code. This also means they should be post-multiplied together in HLSL, so in
  // HLSL code, it would be:
  //     perspective[4x4] * view[4x4] * model[4x4] * position[4x1]
  return XMMatrixMultiply(viewMatrix, perspectiveMatrix);
}

DirectX::XMFLOAT4X4 PinholeCamera::GenerateViewPerspectiveTransform4x4(float aspectRatio) const {
  DirectX::XMFLOAT4X4 viewPerspective4x4;
  DirectX::XMMATRIX viewPerspective = GenerateViewPerspectiveTransform(aspectRatio);
  DirectX::XMStoreFloat4x4(&viewPerspective4x4, viewPerspective);
  return viewPerspective4x4;
}

DirectX::XMMATRIX OrthographicCamera::GenerateViewPerspectiveTransform() const {
  DirectX::XMVECTOR pos = DirectX::XMLoadFloat4(&this->position_);
  DirectX::XMVECTOR look_at = DirectX::XMLoadFloat4(&this->look_at_);
  DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);

  DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookAtLH(pos, look_at, up);
  DirectX::XMMATRIX orthographicMatrix =
      DirectX::XMMatrixOrthographicLH(this->width, this->height, 0.f, 1000.f);

  // Future note: DirectXMath uses row-vector matrices and row-major order for the matrices.
  // This means that matrix multplication with the DirectXMath Library should be done as
  // pre-multiplication. As in, the vector being modified should be on the left. e.g.
  //     position[1x4] * model[4x4] * view[4x4] * perspective[4x4]
  // However, the two cancel out when serializing it and they can be treated as column-vector
  // matrices in HLSL code. This also means they should be post-multiplied together in HLSL, so in
  // HLSL code, it would be:
  //     perspective[4x4] * view[4x4] * model[4x4] * position[4x1]
  return XMMatrixMultiply(viewMatrix, orthographicMatrix);
}

DirectX::XMFLOAT4X4 OrthographicCamera::GenerateViewPerspectiveTransform4x4() const {
  DirectX::XMFLOAT4X4 viewPerspective4x4;
  DirectX::XMMATRIX viewPerspective = GenerateViewPerspectiveTransform();
  DirectX::XMStoreFloat4x4(&viewPerspective4x4, viewPerspective);
  return viewPerspective4x4;
}
