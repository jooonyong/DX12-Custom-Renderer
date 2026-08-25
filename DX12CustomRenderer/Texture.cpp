#include "Texture.h"
#include <memory>

Texture::Texture(Microsoft::WRL::ComPtr<ID3D12Resource> Resource, const D3D12DescriptorHandle& SRV, UINT Width, UINT Height, DXGI_FORMAT Format)
	:Resource(std::move(Resource)), SRV(SRV), Width(Width), Height(Height), Format(Format)
{
}

ID3D12Resource* Texture::GetResource()
{
	return Resource.Get();
}

const D3D12DescriptorHandle& Texture::GetSRV() const
{
	return SRV;
}

UINT Texture::GetWidth() const
{
	return Width;
}

UINT Texture::GetHeight() const
{
	return Height;
}

DXGI_FORMAT Texture::GetFormat() const
{
	return Format;
}
