#pragma once

#include "D3D12CommandQueue.h"
#include <dxgi1_6.h>
#include <wrl.h>

static constexpr UINT BufferCount = 3;

class D3D12SwapChain
{
public:
	bool Initialize(D3D12Device* Device, D3D12CommandQueue* CommandQueue, HWND Hwnd, UINT32 Width, UINT32 Height);

	void Present();
	UINT32 GetBackBufferIndex() const;
	IDXGISwapChain4* GetNativeSwapChain() const { return SwapChain.Get(); }
	ID3D12Resource* GetCurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const;
private:
	Microsoft::WRL::ComPtr<IDXGISwapChain4> SwapChain;
	ID3D12Resource* BackBuffers[BufferCount];
	ID3D12DescriptorHeap* RTVHeap;
	UINT RTVDescriptorSize = 0;
};