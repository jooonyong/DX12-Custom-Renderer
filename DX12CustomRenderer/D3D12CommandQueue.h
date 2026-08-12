#pragma once

#include "D3D12Device.h"

class D3D12CommandContext;;

class D3D12CommandQueue
{
public:
	~D3D12CommandQueue();

	bool Initialize(D3D12Device* Device);

	void Execute(D3D12CommandContext* Command);
	ID3D12CommandQueue* GetNativeCommandQueue() const;

	UINT64 Signal();
	UINT64 GetFenceValue();
	void WaitForFence(UINT64 FenceValue);
	void WaitForIdle();

private:
	ID3D12CommandQueue* CommandQueue;
	ID3D12Fence* Fence;
	HANDLE FenceEvent = nullptr;
	UINT64 FenceValue = 0;

};