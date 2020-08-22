#include "d3d12/ImageLoader.h"

#include "d3d12/comhelper.h"

#include <wincodec.h>
#include <wrl/client.h>  // For ComPtr

using namespace Microsoft::WRL;

HRESULT Image::LoadImageFile(const std::wstring& file, Image* img) {
  ComPtr<IWICImagingFactory> factory = nullptr;
  HRESULT hr = S_OK;

  RETURN_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&factory)));

  ComPtr<IWICBitmapDecoder> decoder = nullptr;
  RETURN_IF_FAILED(factory->CreateDecoderFromFilename(
      file.c_str(),                    // Image to be decoded
      NULL,                            // Do not prefer a particular vendor
      GENERIC_READ,                    // Desired read access to the file
      WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
      &decoder                         // Pointer to the decoder
      ));

  // Retrieve the first frame of the image from the decoder
  ComPtr<IWICBitmapFrameDecode> frame = nullptr;
  RETURN_IF_FAILED(decoder->GetFrame(0, &frame));

  // Make sure the image is in (or can be converted to) R8G8B8A format.
  ComPtr<IWICFormatConverter> converter;
  RETURN_IF_FAILED(factory->CreateFormatConverter(&converter));

  WICPixelFormatGUID format;
  RETURN_IF_FAILED(frame->GetPixelFormat(&format));

  BOOL canConvert;
  RETURN_IF_FAILED(converter->CanConvert(format, GUID_WICPixelFormat32bppRGBA, &canConvert));

  if (!canConvert)
    return E_FAIL;

  UINT width;
  UINT height;
  RETURN_IF_FAILED(converter->GetSize(&width, &height));

  const UINT bytesPerPixel = 4;  // 24bpp / 8.
  std::vector<unsigned char> data(width * height * bytesPerPixel);
  RETURN_IF_FAILED(converter->CopyPixels(nullptr, width * bytesPerPixel, data.size(), data.data()));

  img->data = std::move(data);
  img->format = DXGI_FORMAT_R8G8B8A8_UNORM;
  img->width = width;
  img->height = height;

  return S_OK;
}
