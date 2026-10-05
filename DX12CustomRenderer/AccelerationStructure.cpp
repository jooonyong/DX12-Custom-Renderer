#include "AccelerationStructure.h"

bool AccelerationStructure::CreateScratchBuffer(ID3D12Device* Device, UINT64 SizeInBytes)
{
	D3D12_RESOURCE_DESC ScratchBufferDesc{};
	ScratchBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ScratchBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	ScratchBufferDesc.MipLevels = 1;
	ScratchBufferDesc.Alignment = 0;
	ScratchBufferDesc.Width = SizeInBytes;
	ScratchBufferDesc.Height = 1;
	ScratchBufferDesc.DepthOrArraySize = 1;
	ScratchBufferDesc.SampleDesc = { 1,0 };
	ScratchBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ScratchBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES HeapProp{};
	HeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT Result = Device->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &ScratchBufferDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr, IID_PPV_ARGS(&ScratchBuffer));
	if (FAILED(Result))
	{
		return false;
	}
	return true;
}
bool AccelerationStructure::CreateResultBuffer(ID3D12Device* Device, UINT64 SizeInBytes)
{

}

bool BottomLevelAccelerationStructure::BuildBLAS(D3D12Device* Device, D3D12CommandContext* CommandContext, Mesh* Mesh)
{
	//Raytracing GeometryDesc
	D3D12_RAYTRACING_GEOMETRY_DESC GeometryDesc{};
	GeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	GeometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	GeometryDesc.Triangles.VertexBuffer.StartAddress = Mesh->GetVertexBuffer()->GetGPUVirtualAddress();
	GeometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
	GeometryDesc.Triangles.VertexCount = Mesh->GetVertexBufferView().SizeInBytes / Mesh->GetVertexBufferView().StrideInBytes;
	GeometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	GeometryDesc.Triangles.IndexBuffer = Mesh->GetIndexBuffer()->GetGPUVirtualAddress();
	GeometryDesc.Triangles.IndexCount = Mesh->GetIndexCount();
	GeometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	GeometryDesc.Triangles.Transform3x4 = 0;

	//Build Input
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Inputs{};
	Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	Inputs.NumDescs = 1;
	Inputs.pGeometryDescs = &GeometryDesc;
	
	//Prebuild Info
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo{};
	
	Device->GetRaytracingDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&Inputs, &PrebuildInfo);

	if (!CreateScratchBuffer(Device->GetRaytracingDevice(), PrebuildInfo.ScratchDataSizeInBytes))
	{
		return false;
	}
	if (!CreateResultBuffer(Device->GetRaytracingDevice(), PrebuildInfo.ResultDataMaxSizeInBytes))
	{
		return false;
	}
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BuildDesc{};
	BuildDesc.Inputs = Inputs;	BuildDesc.ScratchAccelerationStructureData = ScratchBuffer->GetGPUVirtualAddress();
	BuildDesc.DestAccelerationStructureData = BLASBuffer->GetGPUVirtualAddress();

	CommandContext->GetRaytracingCommandList()->BuildRaytracingAccelerationStructure(&BuildDesc, 0, nullptr);
	
	D3D12_RESOURCE_BARRIER Barrier{};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	Barrier.UAV.pResource = BLASBuffer.Get();

	CommandContext->GetRaytracingCommandList()->ResourceBarrier(1, &Barrier);
	return true;
}

bool BottomLevelAccelerationStructure::CreateResultBuffer(ID3D12Device* Device, UINT64 SizeInBytes)
{
	D3D12_RESOURCE_DESC ResultBufferDesc{};
	ResultBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResultBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResultBufferDesc.MipLevels = 1;
	ResultBufferDesc.Alignment = 0;
	ResultBufferDesc.Width = SizeInBytes;
	ResultBufferDesc.Height = 1;
	ResultBufferDesc.DepthOrArraySize = 1;
	ResultBufferDesc.SampleDesc = { 1,0 };
	ResultBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResultBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES HeapProp{};
	HeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT Result = Device->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &ResultBufferDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nullptr, IID_PPV_ARGS(&BLASBuffer));
	if (FAILED(Result))
	{
		return false;
	}
	return true;
}

bool TopLevelAccelerationStructure::BuildTLAS(D3D12Device* Device, D3D12CommandContext* CommandContext, const Scene& Scene)
{
	const auto& Objects = Scene.GetRenderObjects();

	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> InstanceDescs;
	InstanceDescs.resize(Objects.size());

	for (UINT i = 0; i < Objects.size(); i++)
	{
		const RenderObject& Object = Objects[i];
		
		D3D12_RAYTRACING_INSTANCE_DESC& Instance = InstanceDescs[i];
		Instance = {};
		Instance.InstanceID = i;
		Instance.InstanceContributionToHitGroupIndex = 0;
		Instance.InstanceMask = 0xFF;
		Instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		Instance.AccelerationStructure = Object.Model->BLAS->GetBLASBuffer()->GetGPUVirtualAddress();

		DirectX::XMFLOAT4X4 World;// = DirectX::XMLoadFloat4x4(&Object.World);
		DirectX::XMMATRIX M = DirectX::XMLoadFloat4x4(&Object.World);
		DirectX::XMStoreFloat4x4(&World, DirectX::XMMatrixTranspose(M));

		Instance.Transform[0][0] = World._11;
		Instance.Transform[0][1] = World._12;
		Instance.Transform[0][2] = World._13;
		Instance.Transform[0][3] = World._14;

		Instance.Transform[1][0] = World._21;
		Instance.Transform[1][1] = World._22;
		Instance.Transform[1][2] = World._23;
		Instance.Transform[1][3] = World._24;

		Instance.Transform[2][0] = World._31;
		Instance.Transform[2][1] = World._32;
		Instance.Transform[2][2] = World._33;
		Instance.Transform[2][3] = World._34;
	}

	if (!CreateInstanceBuffer(Device->GetRaytracingDevice(), InstanceDescs))
	{
		return false;
	}

	//Build Input
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Inputs{};
	Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	Inputs.NumDescs = static_cast<UINT>(InstanceDescs.size());
	Inputs.InstanceDescs = InstanceBuffer->GetGPUVirtualAddress();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo{};
	Device->GetRaytracingDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&Inputs, &PrebuildInfo);

	if (!CreateScratchBuffer(Device->GetRaytracingDevice(), PrebuildInfo.ScratchDataSizeInBytes))
	{
		return false;
	}
	if (!CreateResultBuffer(Device->GetRaytracingDevice(), PrebuildInfo.ResultDataMaxSizeInBytes))
	{
		return false;
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BuildDesc{};
	BuildDesc.Inputs = Inputs;	BuildDesc.ScratchAccelerationStructureData = ScratchBuffer->GetGPUVirtualAddress();
	BuildDesc.DestAccelerationStructureData = TLASBuffer->GetGPUVirtualAddress();

	CommandContext->GetRaytracingCommandList()->BuildRaytracingAccelerationStructure(&BuildDesc, 0, nullptr);

	D3D12_RESOURCE_BARRIER Barrier{};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	Barrier.UAV.pResource = TLASBuffer.Get();

	CommandContext->GetRaytracingCommandList()->ResourceBarrier(1, &Barrier);
	return true;
}

bool TopLevelAccelerationStructure::CreateInstanceBuffer(ID3D12Device* Device, std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& Descs)
{
	UINT64 BufferSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC)* Descs.size();

	D3D12_RESOURCE_DESC InstanceBufferDesc{};
	InstanceBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	InstanceBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	InstanceBufferDesc.MipLevels = 1;
	InstanceBufferDesc.Alignment = 0;
	InstanceBufferDesc.Width = BufferSize;
	InstanceBufferDesc.Height = 1;
	InstanceBufferDesc.DepthOrArraySize = 1;
	InstanceBufferDesc.SampleDesc = { 1,0 };
	InstanceBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	InstanceBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES HeapProp{};
	HeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

	HRESULT Result = Device->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &InstanceBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&InstanceBuffer));
	if (FAILED(Result))
	{
		return false;
	}

	void* MappedData;
	Result = InstanceBuffer->Map(0, nullptr, &MappedData);
	if (FAILED(Result))
	{
		return false;
	}

	memcpy(MappedData, InstanceBuffer.Get(), BufferSize);
	InstanceBuffer->Unmap(0, nullptr);

	return true;
}


bool TopLevelAccelerationStructure::CreateResultBuffer(ID3D12Device* Device, UINT64 SizeInBytes)
{
	D3D12_RESOURCE_DESC ResultBufferDesc{};
	ResultBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResultBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResultBufferDesc.MipLevels = 1;
	ResultBufferDesc.Alignment = 0;
	ResultBufferDesc.Width = SizeInBytes;
	ResultBufferDesc.Height = 1;
	ResultBufferDesc.DepthOrArraySize = 1;
	ResultBufferDesc.SampleDesc = { 1,0 };
	ResultBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResultBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES HeapProp{};
	HeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT Result = Device->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &ResultBufferDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nullptr, IID_PPV_ARGS(&TLASBuffer));
	if (FAILED(Result))
	{
		return false;
	}
	return true;
}