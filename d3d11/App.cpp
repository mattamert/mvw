#include <assert.h>
#include <string>
#include <iostream>
#include <vector>

#include <d3dcompiler.h>

#include "App.h"
#include "Helper.h"

App::App() {
}

App::~App() {
  SafeRelease(&rasterizer_state_);
}

void App::DrawScene() {
  // Set render state and render target.
  m_d3dContext->RSSetState(this->rasterizer_state_);
  m_d3dContext->OMSetRenderTargets(1, &this->m_d3dRenderTargetView, this->m_d3dDepthStencilView);

  // Clear both the render target and the depth stencil buffer.
  m_d3dContext->ClearRenderTargetView(this->m_d3dRenderTargetView, background_color_);
  m_d3dContext->ClearDepthStencilView(this->m_d3dDepthStencilView, D3D11_CLEAR_DEPTH, 1.f, 0);

  // Update the pass's transform.
  DirectX::XMMATRIX m = camera_.GenerateViewPerspectiveTransform();
  DirectX::XMFLOAT4X4 m_4x4;
  DirectX::XMStoreFloat4x4(&m_4x4, m);
  pass_.UpdateViewPerspectiveTransform(m_d3dContext, &m_4x4);

  DirectX::XMFLOAT4X4 world_transform = cube_.GenerateWorldTransform();
  pass_.UpdateWorldTransform(m_d3dContext, &world_transform);

  // Draw the cube.
  pass_.Draw(m_d3dContext, &cube_);
}

void App::CreateDeviceIndependentResources()
{
  // Set up the scene.
  camera_.position_ = DirectX::XMFLOAT4(-8.f, 2.f, 0.f, 1.f);
  camera_.look_at_ = DirectX::XMFLOAT4(0.f, 0.f, 0.f, 1.f);

  cube_.position_ = DirectX::XMFLOAT3(0.f, 0.f, 0.f);
  cube_.rotation_y_ = 0.f;
  cube_.scale_ = 1.f;

  camera_pos_animation_ = AnimationManager::CreateAnimation(5000, true, false);
  cube_pos_animation_ = AnimationManager::CreateAnimation(1000, true, true);
}

void App::CreateDeviceResources()
{
  HR(cube_.Initialize(m_d3dDevice));
  HR(pass_.Initialize(m_d3dDevice));

  D3D11_RASTERIZER_DESC rasterizer_desc;
  ZeroMemory(&rasterizer_desc, sizeof(D3D11_RASTERIZER_DESC));
  rasterizer_desc.FillMode = D3D11_FILL_SOLID;
  rasterizer_desc.CullMode = D3D11_CULL_NONE;
  rasterizer_desc.FrontCounterClockwise = false;
  rasterizer_desc.DepthClipEnable = true;

  HR(m_d3dDevice->CreateRasterizerState(&rasterizer_desc, &rasterizer_state_));
}

void App::CreateWindowSizeDependentResources()
{
  this->camera_.aspect_ratio_ = static_cast<float>(this->m_windowBounds.width) / static_cast<float>(this->m_windowBounds.height);
}

void App::Animate() {
  //float progress_pos = AnimationManager::TickAnimation(camera_pos_animation_);
  //camera_.position_ = DirectX::XMFLOAT4(
  //  -8 * std::cos(progress_pos * 2 * DirectX::XM_PI),
  //  0.f,
  //  8 * std::sin(progress_pos * 2 * DirectX::XM_PI),
  //  1.f);

  float progress_cube_pos = AnimationManager::TickAnimation(cube_pos_animation_);
  cube_.position_.x = progress_cube_pos * 2;
  cube_.rotation_y_ = 2 * DirectX::XM_PI * progress_cube_pos;

}
