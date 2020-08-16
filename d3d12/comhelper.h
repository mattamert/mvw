#pragma once

//#if defined(_DEBUG)
#if 1
//#define HR(hr) if (!SUCCEEDED(hr)) { __debugbreak(); throw; }

inline void HR(HRESULT hr) {
  if (!SUCCEEDED(hr)) {
    __debugbreak();
    throw;
  }
}
#else
#define HR(hr)          \
  if (!SUCCEEDED(hr)) { \
    throw;              \
  }
#endif

template <class T>
inline void SafeRelease(T** comInterfaceToRelease) {
  if (*comInterfaceToRelease != nullptr) {
    (*comInterfaceToRelease)->Release();
    (*comInterfaceToRelease) = nullptr;
  }
}
