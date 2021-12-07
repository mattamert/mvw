#pragma once

#include "d3d12/Animation.h"
#include "d3d12/Camera.h"
#include "d3d12/DescriptorHeapManagers.h"
#include "d3d12/ResourceGarbageCollector.h"
#include "d3d12/Object.h"

#include <string>

class Scene {
  std::string m_objFilename;
  Object m_object;
  Animation m_objectRotationAnimation;

  PinholeCamera m_camera;

public:
  void Initialize(const std::string& objFilename,
                  ID3D12Device* device,
                  ID3D12GraphicsCommandList* cl,
                  LinearDescriptorAllocator& srvAllocator,
                  ResourceGarbageCollector& garbageCollector,
                  size_t nextFenceValue);

  void TickAnimations();

  // Note: can't be named GetObject because it's apparently a Windows macro :/
  Object& GetObj();
  const PinholeCamera& GetCamera() const;
};