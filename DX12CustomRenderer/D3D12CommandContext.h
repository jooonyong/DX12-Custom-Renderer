#pragma once

#include "D3D12CommandQueue.h"

class D3D12CommandContext
{
public:
	~D3D12CommandContext();
	bool Initialize(D3D12Device* Device, ID3D12CommandAllocator* CommandAllocator);

	bool Reset(ID3D12CommandAllocator* CommandAllocator);

	bool Close();

	ID3D12GraphicsCommandList* GetCommandList() const { return CommandList; }
	ID3D12GraphicsCommandList4* GetRaytracingCommandList() const { return RaytracingCommandList; }	

private:
	ID3D12GraphicsCommandList* CommandList = nullptr;;
	ID3D12GraphicsCommandList4* RaytracingCommandList = nullptr;
};