#include "d3d12/Scene.h"

#include "d3d12/D3D12Renderer.h"

void Scene::Initialize(const std::string& objFilename, D3D12Renderer* renderer) {
  Model model;
  model.InitFromObjFile(renderer, objFilename);

  const ObjData::AxisAlignedBounds& bounds = model.GetBounds();
  float width = std::abs(bounds.max[0] - bounds.min[0]);
  float height = std::abs(bounds.max[1] - bounds.min[1]);
  float length = std::abs(bounds.max[2] - bounds.min[2]);

  // TODO: This syntax is weird, but I don't want to have to deal with windows headers right now.
  // Ideally, we'd just define NOMINMAX as a compiler flag.
  float maxDimension = (std::max)(width, (std::max)(height, length));

  m_object.model = std::move(model);
  m_object.position = DirectX::XMFLOAT4(0, 0, 0, 1);
  m_object.rotationY = 0;
  m_object.scale = 1 / maxDimension;  // Scale such that the max dimension is of height 1.

  m_objectRotationAnimation = Animation::CreateAnimation(10000, /*repeat*/ true);

  m_camera.position_ = DirectX::XMFLOAT4(1.25, 0.25, 1.25, 1.f);
  m_camera.look_at_ = DirectX::XMFLOAT4(0, 0, 0, 1);
}

void Scene::TickAnimations() {
  double progress = Animation::TickAnimation(m_objectRotationAnimation);
  m_object.rotationY = progress * 2 * 3.14159265;
}

Object& Scene::GetObj() {
  return m_object;
}

const PinholeCamera& Scene::GetCamera() const {
  return m_camera;
}
