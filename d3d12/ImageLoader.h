#pragma once

#include <string>
#include <vector>

#include <Windows.h>
#include <dxgiformat.h>

struct Image {
  static HRESULT LoadImageFile(const std::wstring& file, Image* img);

  std::vector<unsigned char> data;
  DXGI_FORMAT format;
  size_t width;
  size_t height;

  size_t bytesPerPixel;  // TODO: Doing it this way does not support bc-packed textures.
};
