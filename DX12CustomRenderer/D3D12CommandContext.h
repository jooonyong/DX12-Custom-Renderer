#pragma once

#include "D3D12CommandQueue.h"

struct FrameResource
{
	ID3D12CommandAllocator* CommandAllocator = nullptr;
	UINT FenceValue = 0;
	ID3D12Resource* ConstantBuffer = nullptr;
	void* ConstantBufferMappedData = nullptr;
};

class D3D12CommandContext
{
public:
	~D3D12CommandContext();
	bool Initialize(D3D12Device* Device, ID3D12CommandAllocator* CommandAllocator);

	bool Reset(ID3D12CommandAllocator* CommandAllocator);

	bool Close();

	ID3D12GraphicsCommandList* GetCommandList() const { return CommandList; }

private:
	ID3D12GraphicsCommandList* CommandList;
};