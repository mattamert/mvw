#pragma once

#include <string>
#include <vector>

#include <dxgiformat.h>
#include <Windows.h>

struct Image {
  static HRESULT LoadImageFile(const std::wstring& file, Image* img);

  std::vector<unsigned char> data;
  DXGI_FORMAT format;
  size_t width;
  size_t height;
};
