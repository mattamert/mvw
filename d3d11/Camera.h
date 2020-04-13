#pragma once

#include <DirectXMath.h>

class PinholeCamera {
public:
  DirectX::XMMATRIX GenerateViewPerspectiveTransform() const;

  DirectX::XMFLOAT4 position_;
  DirectX::XMFLOAT4 look_at_;
  float aspect_ratio_;
};
