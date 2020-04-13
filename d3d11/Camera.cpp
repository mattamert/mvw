#include "Camera.h"

DirectX::XMMATRIX PinholeCamera::GenerateViewPerspectiveTransform() const {
  DirectX::XMVECTOR pos = DirectX::XMLoadFloat4(&this->position_);
  DirectX::XMVECTOR look_at = DirectX::XMLoadFloat4(&this->look_at_);
  DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);

  DirectX::XMMATRIX view_matrix = DirectX::XMMatrixLookAtLH(pos, look_at, up);
  DirectX::XMMATRIX perspective_matrix = DirectX::XMMatrixPerspectiveFovLH(0.25f * DirectX::XM_PI, this->aspect_ratio_, 0.1f, 1000.f);

  return XMMatrixMultiply(view_matrix, perspective_matrix);
}
