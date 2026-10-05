#include "AccelerationStructure.h"

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
	Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	Inputs.NumDescs = 1;
	Inputs.pGeometryDescs = &GeometryDesc;
	
	//Prebuild Info
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo;
	
	Device->GetRaytracingDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&Inputs, &PrebuildInfo);

	if (FAILED(CreateScratchBuffer(Device->GetRaytracingDevice(), PrebuildInfo.ScratchDataSizeInBytes)))
	{
		return false;
	}
	if (FAILED(CreateResultBuffer(Device->GetRaytracingDevice(), PrebuildInfo.ResultDataMaxSizeInBytes)))
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

	return true;
}

bool BottomLevelAccelerationStructure::CreateScratchBuffer(ID3D12Device* Device, UINT64 SizeInBytes)
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
