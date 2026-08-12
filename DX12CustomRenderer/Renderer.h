#pragma once

#include "D3D12CommandQueue.h"
#include "D3D12CommandContext.h"
#include "D3D12SwapChain.h"

class Renderer
{
public:
	bool Initialize(HWND Hwnd, UINT Width, UINT Height);

	void RenderFrame();

private:
	D3D12Device Device;
	D3D12CommandQueue CommandQueue;
	D3D12CommandContext CommandContext;
	D3D12SwapChain SwapChain;
};