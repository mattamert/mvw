#include <d3d12.h>
#include <Windows.h>
#include <wrl/client.h> // For ComPtr

#include <string>

class DXWindow {
private:
  bool isInitialized;
  HWND hwnd;

  Microsoft::WRL::ComPtr<ID3D12Device> device;

public:
  void Initialize();

  void OnResize(unsigned int width, unsigned int height);
  void DrawScene();
  void Present();

private:
  static void RegisterDXWindow();
  static HWND CreateDXWindow(DXWindow* window, const std::wstring& windowName, int width, int height);
};
