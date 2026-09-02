#include "Material.h"

Material::Material(std::shared_ptr<Texture> AlbedoTexture, std::shared_ptr<Texture> MRTexture, const DirectX::XMFLOAT4& BaseColor, float Roughness, float Metallic)
	:AlbedoTexture(std::move(AlbedoTexture)), MetallicRoughnessTexture(std::move(MRTexture))
{
	Constants.BaseColor = BaseColor;
	Constants.Roughness = Roughness;
	Constants.Metallic = Metallic;	
}

Material::~Material()
{
	for (auto FrameResource : FrameResources)
	{
		if (FrameResource.ConstantBuffer && FrameResource.MappedData)
		{
			FrameResource.ConstantBuffer->Unmap(0, nullptr);
			FrameResource.MappedData = nullptr;
		}
	}
}

bool Material::InitializeGPU(ID3D12Device* Device, UINT FrameCount)
{
	FrameResources.resize(FrameCount);

	//ConstantBuffer의 시작 주소와 전체 크기는 **항상 256바이트의 배수(256바이트 정렬)**여야한다.
	//GPU는 256바이트 단위로 정렬된 메모리 주소에 접근할 때 성능이 최적화되도록 설계되어 있다.
	//크기 계산 공식 : C++ 코드에서 상수 버퍼 뷰(CBV)를 생성할 때 크기는 보통 아래와 같이 256바이트 배수로 올림(Padding) 처리합니다.
	//sizeInBytes = (sizeof(MyConstantBuffer) + 255) & ~255; 
	//하위 8비트(0~255)를 0처리함으로써 256의 배수로 올림처리 하는 코드
	const UINT64 BufferSize = (sizeof(MaterialConstants) + 255) & ~255;
	
	for (int i = 0; i < FrameCount; i++)
	{
		MaterialFrameResource& Frame = FrameResources[i];

		D3D12_RESOURCE_DESC BufferDesc{};
		BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		BufferDesc.MipLevels = 1;
		BufferDesc.DepthOrArraySize = 1;
		BufferDesc.Width = BufferSize;
		BufferDesc.Height = 1;
		BufferDesc.SampleDesc.Count = 1;
		BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		D3D12_HEAP_PROPERTIES HeapProp{};
		HeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

		HRESULT Result = Device->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Frame.ConstantBuffer));
		if (FAILED(Result))
		{
			return false;
		}

		D3D12_RANGE ReadRange{};
		ReadRange.Begin = 0;
		ReadRange.End = 0;

		Result = Frame.ConstantBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&Frame.MappedData));
		if (FAILED(Result))
		{
			return false;
		}
		memcpy(Frame.MappedData, &Constants, sizeof(MaterialConstants));
	}

	return true;
}

void Material::UpdateGPU(UINT FrameIndex)
{
	memcpy(FrameResources[FrameIndex].MappedData, &Constants,sizeof(MaterialConstants));
}

const std::shared_ptr<Texture>& Material::GetAlbedoTexture() const
{
	return AlbedoTexture;
}

const std::shared_ptr<Texture>& Material::GetMetallicRoughnessTexture() const
{
	return MetallicRoughnessTexture;
}

const DirectX::XMFLOAT4 Material::GetBaseColor() const
{
	return Constants.BaseColor;
}

float Material::GetRoughness() const
{
	return Constants.Roughness;
}

float Material::GetMetallic() const
{
	return Constants.Metallic;
}

void Material::SetBaseColor(DirectX::XMFLOAT4& BaseColor)
{
	Constants.BaseColor = BaseColor;
}

void Material::SetRoughness(float Roughness)
{
	Constants.Roughness = Roughness;
}

void Material::SetMetallic(float Metallic)
{
	Constants.Metallic = Metallic;
}

D3D12_GPU_VIRTUAL_ADDRESS Material::GetConstantBufferGPUAddress(UINT FrameIndex) const
{
	return FrameResources[FrameIndex].ConstantBuffer->GetGPUVirtualAddress();
}
