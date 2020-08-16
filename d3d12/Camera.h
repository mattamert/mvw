#pragma once

#include <DirectXMath.h>

class PinholeCamera {
public:
  DirectX::XMMATRIX GenerateViewPerspectiveTransform(float aspectRatio) const;

  DirectX::XMFLOAT4 position_;
  DirectX::XMFLOAT4 look_at_;
};
