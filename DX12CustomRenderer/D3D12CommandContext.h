#pragma once

#include "D3D12CommandQueue.h"

class D3D12CommandContext
{
public:
	bool Initialize(D3D12Device* Device);

	bool Reset();

	bool Close();

	ID3D12CommandAllocator* GetCommandAllocator() const { return CommandAllocator; }
	ID3D12GraphicsCommandList* GetCommandList() const { return CommandList; }

private:
	ID3D12CommandAllocator* CommandAllocator;;
	ID3D12GraphicsCommandList* CommandList;
};