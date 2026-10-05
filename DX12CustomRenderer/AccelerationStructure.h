#pragma once

#include "D3D12CommandContext.h"
#include "Mesh.h"

class BottomLevelAccelerationStructure
{
public:
	bool BuildBLAS(D3D12Device* Device, D3D12CommandContext* CommandContext, Mesh* Mesh);
	
	bool CreateScratchBuffer(ID3D12Device* Device, UINT64 SizeInBytes);
	bool CreateResultBuffer(ID3D12Device* Device, UINT64 SizeInBytes);

	Microsoft::WRL::ComPtr<ID3D12Resource> GetBLASBuffer() const { return BLASBuffer.Get(); }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> ScratchBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> BLASBuffer;
};