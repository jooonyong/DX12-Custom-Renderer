#pragma once

#include "D3D12CommandQueue.h"

struct FrameResource
{
	ID3D12CommandAllocator* CommandAllocator;
	UINT FenceValue = 0;
};

class D3D12CommandContext
{
public:
	bool Initialize(D3D12Device* Device, ID3D12CommandAllocator* CommandAllocator);

	bool Reset(ID3D12CommandAllocator* CommandAllocator);

	bool Close();

	ID3D12GraphicsCommandList* GetCommandList() const { return CommandList; }

private:
	ID3D12GraphicsCommandList* CommandList;
};