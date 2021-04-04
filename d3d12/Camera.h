#pragma once

#include <DirectXMath.h>

class PinholeCamera {
 public:
  DirectX::XMMATRIX GenerateViewPerspectiveTransform(float aspectRatio) const;
  DirectX::XMFLOAT4X4 GenerateViewPerspectiveTransform4x4(float aspectRatio) const;

  DirectX::XMFLOAT4 position_;
  DirectX::XMFLOAT4 look_at_;
};

// TODO: Should probably make base class Camera.
class OrthographicCamera {
public:
  DirectX::XMMATRIX GenerateViewPerspectiveTransform() const;
  DirectX::XMFLOAT4X4 GenerateViewPerspectiveTransform4x4() const;

  DirectX::XMFLOAT4 GetLightDirection() const;

  DirectX::XMFLOAT4 position_;
  DirectX::XMFLOAT4 look_at_;
  float widthInWorldCoordinates;
  float heightInWorldCoordinates;
};
