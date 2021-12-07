#include "d3d12/Object.h"

DirectX::XMMATRIX Object::GenerateModelTransform() const {
  const ObjFileData::AxisAlignedBounds bounds = this->model.GetBounds();
  float midpoint[3];
  midpoint[0] = (bounds.max[0] + bounds.min[0]) / 2;
  midpoint[1] = (bounds.max[1] + bounds.min[1]) / 2;
  midpoint[2] = (bounds.max[2] + bounds.min[2]) / 2;

  DirectX::XMMATRIX translationToOrigin = DirectX::XMMatrixTranslation(-midpoint[0], -midpoint[1], -midpoint[2]);

  DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(this->scale, this->scale, this->scale);
  DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationY(this->rotationY);
  DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(this->position.x, this->position.y, this->position.z);

  DirectX::XMMATRIX modelTransform = translationToOrigin * scale * rotation * translation;
  return modelTransform;
}

DirectX::XMFLOAT4X4 Object::GenerateModelTransform4x4() const {
  DirectX::XMFLOAT4X4 modelTransform4x4;
  DirectX::XMMATRIX modelTransform = GenerateModelTransform();
  DirectX::XMStoreFloat4x4(&modelTransform4x4, modelTransform);
  return modelTransform4x4;
}
