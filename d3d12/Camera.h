#pragma once

#include <DirectXMath.h>

struct PinholeCamera {
  DirectX::XMFLOAT4 position_;
  DirectX::XMFLOAT4 look_at_;

  DirectX::XMMATRIX GenerateViewPerspectiveTransform(float aspectRatio) const;
  DirectX::XMFLOAT4X4 GenerateViewPerspectiveTransform4x4(float aspectRatio) const;
};

// TODO: Should probably make base class Camera.
struct OrthographicCamera {
  DirectX::XMFLOAT4 position_;
  DirectX::XMFLOAT4 look_at_;
  float widthInWorldCoordinates;
  float heightInWorldCoordinates;

  DirectX::XMMATRIX GenerateViewPerspectiveTransform() const;
  DirectX::XMFLOAT4X4 GenerateViewPerspectiveTransform4x4() const;
  DirectX::XMFLOAT4 GetLightDirection() const;
};

struct ArcballCameraController {
  // For now, instead of trying to do a proper "Arcball" (which in my mind is a sphere that you manipulate with the
  // mouse to alter your viewing angle), we just convert deltaX & deltaY to movement in the sphereical coordinate
  // system.
  // The nice part about this is that the camera is always oriented in a reasonable way, which I've found to not be the
  // case in a true Arcball controller.
  DirectX::XMFLOAT4 m_arcballCenter;
  float m_rotationXInDegrees;
  float m_rotationYInDegrees;
  float m_distance;

  void OnMouseDrag(int deltaX, int deltaY);
  void OnMouseWheel(float wheelDelta);
  PinholeCamera GetPinholeCamera() const; // This is kinda jank, but it works for now..
};
