#pragma once

#include <DirectXMath.h>
#include <memory>
#include <vector>
#include "Texture.h"

struct MaterialFrameResource
{
	Microsoft::WRL::ComPtr<ID3D12Resource> ConstantBuffer;
	uint8_t* MappedData = nullptr;
};

struct MaterialConstants
{
	DirectX::XMFLOAT4 BaseColor = { 0.8f, 0.15f, 0.05f, 1.0f };
	float Roughness = 0.15f;
	float Metallic = 0.0f;

	//HLSL Constant Buffer의 packing을 맞추기 위해서
	DirectX::XMFLOAT2 Padding = { 0.0f, 0.0f };
};

class Material
{
public:
	Material(std::shared_ptr<Texture> AlbedoTexture, const DirectX::XMFLOAT4& BaseColor, float Roughness, float Metallic);
	~Material();

	bool InitializeGPU(ID3D12Device* Device, UINT FrameCount);
	void UpdateGPU(UINT FrameIndex);

	const std::shared_ptr<Texture>& GetAlbedoTexture() const;
	const DirectX::XMFLOAT4 GetBaseColor() const;

	float GetRoughness() const;
	float GetMetallic() const;

	void SetBaseColor(DirectX::XMFLOAT4& BaseColor);
	void SetRoughness(float Roughness);
	void SetMetallic(float Metallic);

	D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferGPUAddress(UINT FrameIndex) const;

private:
	//여러 Material이 같은 Texture를 사용할 수 있기 떄문에 unique_ptr말고 shared_ptr사용
	std::shared_ptr<Texture> AlbedoTexture = nullptr;
	
	MaterialConstants Constants;
	std::vector<MaterialFrameResource> FrameResources;

	/*Microsoft::WRL::ComPtr<ID3D12Resource> ConstantBuffer = nullptr;
	uint8_t* ConstantBufferMappedData = nullptr;*/
};