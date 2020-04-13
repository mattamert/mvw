#pragma once

#include <d3d11_1.h>
#include <DirectXMath.h>

#include "GPUMesh.h"

class Object {
public:
  virtual ~Object() {};
  virtual HRESULT Initialize(ID3D11Device1* device) = 0;

  DirectX::XMFLOAT4X4 GenerateWorldTransform();

  GPUMesh mesh;
  DirectX::XMFLOAT3 position_;
  float rotation_y_;
  float scale_;
};


struct ShadedVertex {
  float pos[3];
  float normal[3];
};

class CubeObject : public Object {
public:
  ~CubeObject() {}
  HRESULT Initialize(ID3D11Device1* device) override;
};

class SphereObject : public Object {
public:
  ~SphereObject() {}
  HRESULT Initialize(ID3D11Device1* device) override;
};
