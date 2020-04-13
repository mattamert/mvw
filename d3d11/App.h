#pragma once

#include <dwrite.h>
#include <dwrite_3.h>

#include "AnimationManager.h"
#include "Camera.h"
#include "DirectXBase.h"
#include "GPUMesh.h"
#include "Object.h"
#include "Pass.h"

class App : public DirectXBase
{
public:
  App();
  ~App();

  virtual void DrawScene() override;
  virtual void CreateDeviceIndependentResources() override;
  virtual void CreateDeviceResources() override;
  virtual void CreateWindowSizeDependentResources() override;

  void Animate() override;

  PinholeCamera camera_;
  CubeObject cube_;
  DefaultPass pass_;

  // Note that constant buffers must be specified in multiples of 16 bytes. So even though we need only 1 float, we specify 4 so that we don't upload garbage data.
  Animation camera_pos_animation_;
  Animation cube_pos_animation_;

  ID3D11RasterizerState* rasterizer_state_;

  float background_color_[4] = { 0.39f, 0.58f, 0.92f, 1.0f };
};