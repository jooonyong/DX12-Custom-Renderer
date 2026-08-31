#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "D3D12DescriptorAllocator.h"

enum class TextureColorSpace
{
	Linear,
	SRGB
};

class Texture
{
public:
	Texture(Microsoft::WRL::ComPtr<ID3D12Resource> Resource, const D3D12DescriptorHandle& SRV, UINT Width, UINT Height, DXGI_FORMAT Format);

	ID3D12Resource* GetResource();

	const D3D12DescriptorHandle& GetSRV() const;

	UINT GetWidth() const;
	UINT GetHeight() const;
	DXGI_FORMAT GetFormat() const;

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource;

	D3D12DescriptorHandle SRV;

	UINT Width = 0;
	UINT Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;

};