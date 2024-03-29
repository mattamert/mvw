#include "d3d12/Camera.h"

#include <algorithm>
#include <cmath>

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
      DirectX::XMMatrixOrthographicLH(this->widthInWorldCoordinates, this->heightInWorldCoordinates, 0.f, 5.f);

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

DirectX::XMFLOAT4 OrthographicCamera::GetLightDirection() const {
  // Don't know if it's faster to do this as opposed to just directly calculating it with XMFLOAT4s,
  // but like, this way we can claim that we're making full use of SIMD instructions or something.
  DirectX::XMVECTOR lookAtVector = DirectX::XMLoadFloat4(&look_at_);
  DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat4(&position_);

  // Technically, the w-component is 0 after the subtraction here, but it shouldn't matter; we never
  // need the w component for the light direction.
  DirectX::XMVECTOR lightDirVector = positionVector - lookAtVector;

  DirectX::XMFLOAT4 result;
  DirectX::XMStoreFloat4(&result, lightDirVector);
  return result;
}

void ArcballCameraController::OnMouseDrag(int deltaX, int deltaY) {
  m_rotationXInDegrees += -(float)deltaX / 2.f;
  m_rotationYInDegrees += -(float)deltaY / 2.f;

  m_rotationXInDegrees = std::fmodf(m_rotationXInDegrees, 360.f);
  m_rotationYInDegrees = std::clamp(m_rotationYInDegrees, 10.f, 170.f);
}

void ArcballCameraController::OnMouseWheel(float wheelDelta) {
  m_distance -= 0.1f * wheelDelta;
  m_distance = std::max(0.1f, m_distance); // Minimum of 0.1 distance.
}

PinholeCamera ArcballCameraController::GetPinholeCamera() const {
  const float pi = 3.14159265f;
  float rotationXInRadians = m_rotationXInDegrees * pi / 180.f;
  float rotationYInRadians = m_rotationYInDegrees * pi / 180.f;

  DirectX::XMFLOAT4 cartesianPosition;
  cartesianPosition.x = m_distance * std::sinf(rotationYInRadians) * std::cosf(rotationXInRadians);
  cartesianPosition.y = m_distance * std::cosf(rotationYInRadians);
  cartesianPosition.z = m_distance * std::sinf(rotationYInRadians) * std::sinf(rotationXInRadians);
  cartesianPosition.w = 1.f;

  PinholeCamera pinholeCamera;
  pinholeCamera.position_ = cartesianPosition;
  pinholeCamera.look_at_ = m_arcballCenter;
  return pinholeCamera;
}
