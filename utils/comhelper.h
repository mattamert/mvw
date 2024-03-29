#pragma once

#if defined(_DEBUG)
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

#define RETURN_IF_FAILED(x) \
  hr = x;                   \
  if (FAILED(hr))           \
    return hr;

template <class T>
inline void SafeRelease(T** comInterfaceToRelease) {
  if (*comInterfaceToRelease != nullptr) {
    (*comInterfaceToRelease)->Release();
    (*comInterfaceToRelease) = nullptr;
  }
}
