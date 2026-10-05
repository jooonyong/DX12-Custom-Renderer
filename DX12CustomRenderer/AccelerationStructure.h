#pragma once

#include "D3D12CommandContext.h"
#include "Mesh.h"
#include "Scene.h"

class AccelerationStructure
{
public:
	bool CreateScratchBuffer(ID3D12Device* Device, UINT64 SizeInBytes);
	virtual bool CreateResultBuffer(ID3D12Device* Device, UINT64 SizeInBytes) = 0;

protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> ScratchBuffer;
};

class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
	bool BuildBLAS(D3D12Device* Device, D3D12CommandContext* CommandContext, Mesh* Mesh);
	
	bool CreateResultBuffer(ID3D12Device* Device, UINT64 SizeInBytes) override;

	Microsoft::WRL::ComPtr<ID3D12Resource> GetBLASBuffer() const { return BLASBuffer.Get(); }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> BLASBuffer;
};

class TopLevelAccelerationStructure : public AccelerationStructure
{
public:
	bool BuildTLAS(D3D12Device* Device, D3D12CommandContext* CommandContext, const Scene& Scene);

	bool CreateInstanceBuffer(ID3D12Device* Device, std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& Descs);
	bool CreateResultBuffer(ID3D12Device* Device, UINT64 SizeInBytes) override;

	ID3D12Resource* GetTLASBuffer() const { return TLASBuffer.Get(); }
	bool IsBuilt() const { return bBuilt; }
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> InstanceBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> TLASBuffer;

	bool bBuilt = false;
};