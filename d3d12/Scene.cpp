#include "d3d12/Scene.h"

#include "d3d12/D3D12Renderer.h"

void Scene::Initialize(const std::string& objFilename, bool isTownscaper, D3D12Renderer* renderer) {
  Model model;
  model.InitFromObjFile(renderer, objFilename);
  m_isTownscaper = isTownscaper;

  const ObjFileData::AxisAlignedBounds& bounds = model.GetBounds();
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

  m_shadowMapCamera.position_ = DirectX::XMFLOAT4(-1, 1, 1, 1.f);
  m_shadowMapCamera.look_at_ = DirectX::XMFLOAT4(0, 0, 0, 1);
  m_shadowMapCamera.widthInWorldCoordinates = 1.5;
  m_shadowMapCamera.heightInWorldCoordinates = 1.5;

  m_camera.m_arcballCenter = DirectX::XMFLOAT4(0.f, 0.f, 0.f, 1.f);
  m_camera.m_rotationXInDegrees = 0.f;
  m_camera.m_rotationYInDegrees = 90.f;
  m_camera.m_distance = 1.f;
}

void Scene::TickAnimations() {
  // Disable the rotating animation for now so that it doesn't conflict with mouse movement.
  //double progress = Animation::TickAnimation(m_objectRotationAnimation);
  //m_object.rotationY = progress * 2 * 3.14159265;
}
