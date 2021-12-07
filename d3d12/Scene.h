#pragma once

#include "d3d12/Animation.h"
#include "d3d12/Camera.h"
#include "d3d12/DescriptorHeapManagers.h"
#include "d3d12/ResourceGarbageCollector.h"
#include "d3d12/Object.h"

#include <string>

class D3D12Renderer;

class Scene {
  std::string m_objFilename;
  Object m_object;
  Animation m_objectRotationAnimation;

  PinholeCamera m_camera;

public:
  void Initialize(const std::string& objFilename, D3D12Renderer* renderer);

  void TickAnimations();

  // Note: can't be named GetObject because it's apparently a Windows macro :/
  Object& GetObj();
  const PinholeCamera& GetCamera() const;
};
