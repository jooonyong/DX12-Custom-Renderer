#include "ImageLoader.h"
#include <wincodec.h>
#include <wrl.h>

#pragma comment(lib, "windowscodecs.lib")

bool ImageLoader::LoadFromFile(const std::wstring& FilePath, ImageData& OutImage)
{
	Microsoft::WRL::ComPtr<IWICImagingFactory> Factory;
	HRESULT Result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(Factory.GetAddressOf()));
	if (FAILED(Result))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<IWICBitmapDecoder> Decoder;
	Result = Factory->CreateDecoderFromFilename(FilePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, Decoder.GetAddressOf());
	if (FAILED(Result))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> Frame;
	Result = Decoder->GetFrame(0, Frame.GetAddressOf());
	if (FAILED(Result))
	{
		return false;
	}

	Frame->GetSize(&OutImage.Width, &OutImage.Height);

	Microsoft::WRL::ComPtr<IWICFormatConverter> Converter;
	Result = Factory->CreateFormatConverter(Converter.GetAddressOf());
	if (FAILED(Result))
	{
		return false;
	}
	Result = Converter->Initialize(Frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
		nullptr, 0.0, WICBitmapPaletteTypeCustom);
	if (FAILED(Result))
	{
		return false;
	}

	const UINT BytesPerPixel = 4;
	const UINT RowPitch = OutImage.Width * BytesPerPixel;
	const UINT ImageSize = RowPitch * OutImage.Height;
	OutImage.Pixels.resize(ImageSize);

	Result = Converter->CopyPixels(nullptr, RowPitch, ImageSize, OutImage.Pixels.data());
	if (FAILED(Result))
	{
		return false;
	}
	return true;
}

bool ImageLoader::LoadFromMemory(const uint8_t* Data, size_t Size, ImageData& OutImage)
{
	Microsoft::WRL::ComPtr<IWICStream> Stream;
	Microsoft::WRL::ComPtr<IWICImagingFactory> Factory;
	
	HRESULT Result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(Factory.GetAddressOf()));
	if (FAILED(Result))
	{
		return false;
	}

	if (FAILED(Factory->CreateStream(&Stream)))
	{
		return false;
	}

	Result = Stream->InitializeFromMemory(const_cast<BYTE*>(Data),static_cast<DWORD>(Size));
	if (FAILED(Result))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<IWICBitmapDecoder> Decoder;
	Result = Factory->CreateDecoderFromStream(Stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, Decoder.GetAddressOf());
	if (FAILED(Result))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> Frame;
	Result = Decoder->GetFrame(0, Frame.GetAddressOf());
	if (FAILED(Result))
	{
		return false;
	}

	Frame->GetSize(&OutImage.Width, &OutImage.Height);

	Microsoft::WRL::ComPtr<IWICFormatConverter> Converter;
	Result = Factory->CreateFormatConverter(Converter.GetAddressOf());
	if (FAILED(Result))
	{
		return false;
	}
	Result = Converter->Initialize(Frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
		nullptr, 0.0, WICBitmapPaletteTypeCustom);
	if (FAILED(Result))
	{
		return false;
	}

	const UINT BytesPerPixel = 4;
	const UINT RowPitch = OutImage.Width * BytesPerPixel;
	const UINT ImageSize = RowPitch * OutImage.Height;
	OutImage.Pixels.resize(ImageSize);

	Result = Converter->CopyPixels(nullptr, RowPitch, ImageSize, OutImage.Pixels.data());
	if (FAILED(Result))
	{
		return false;
	}
	return true;
}
