#pragma once

#include <Windows.h>
#include <iostream>

// This is a bullshit file, but it's unfortunately necessary to deal with the
// heaping pile of shit that is COM/WindowsAPI.


//inline void HR(HRESULT hr)
//{
//  if (!SUCCEEDED(hr)) {
//    __debugbreak();
//    std::cerr << "Error." << std::endl;
//    throw;
//  }
//}

//#if defined(_DEBUG)
//#define HR(hr) if (!SUCCEEDED(hr)) { __debugbreak(); throw; }
//#else
//#define HR(hr) (void)hr
//#endif

//#if defined(_DEBUG)
#if 1
#define HR(hr) if (!SUCCEEDED(hr)) { __debugbreak(); throw; }
#else
#define HR(hr) if (!SUCCEEDED(hr)) { throw; }
#endif

template <class T>
inline void SafeRelease(T** comInterfaceToRelease)
{
  if (*comInterfaceToRelease != nullptr)
  {
    (*comInterfaceToRelease)->Release();
    (*comInterfaceToRelease) = nullptr;
  }
}