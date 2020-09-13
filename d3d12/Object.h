#pragma once

#include "d3d12/Model.h"

#include <DirectXMath.h>

class Object {
public:
  Model model;

  DirectX::XMFLOAT4 position;

  // TODO: Eventually I imagine we'll replace this with Quaternions (Or, more generally, with
  // rotors). But that sounds hard, so let's just stick with just 1 rotational axis for now.
  float rotationY; 
  float scale;

  DirectX::XMMATRIX Object::GenerateModelTransform();
};