#include "d3d12/Object.h"

DirectX::XMMATRIX Object::GenerateModelTransform() {
  DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(this->scale, this->scale, this->scale);
  DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationY(this->rotationY);
  DirectX::XMMATRIX translation =
      DirectX::XMMatrixTranslation(this->position.x, this->position.y, this->position.z);

  DirectX::XMMATRIX modelTransform = scale * rotation * translation;
  return modelTransform;
}
