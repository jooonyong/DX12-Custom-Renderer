#include "D3D12SwapChain.h"

D3D12SwapChain::~D3D12SwapChain()
{
	for (int i = 0; i < BufferCount; i++)
	{
		if (BackBuffers[i])
		{
			BackBuffers[i]->Release();
			BackBuffers[i] = nullptr;
		}
	}
	if (RTVHeap)
	{
		RTVHeap->Release();
		RTVHeap = nullptr;
	}
}

bool D3D12SwapChain::Initialize(D3D12Device* Device, D3D12CommandQueue* CommandQueue, HWND Hwnd, UINT32 Width, UINT32 Height)
{
	if(!Device || !Device->GetDevice() || !CommandQueue || !CommandQueue->GetNativeCommandQueue() || !Hwnd)
	{
		return false;
	}

	DXGI_SWAP_CHAIN_DESC1 SwapChainDesc{};
	//DXGI_MODE_DESC SwapChainBufferDesc{};

	//SwapChainBufferDesc.Width = Width;
	//SwapChainBufferDesc.Height = Height;
	//SwapChainBufferDesc.RefreshRate.Numerator = 60;
	//SwapChainBufferDesc.RefreshRate.Denominator = 1;
	//SwapChainBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//SwapChainBufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	//SwapChainBufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	SwapChainDesc.BufferCount = BufferCount;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	SwapChainDesc.Width = Width;
	SwapChainDesc.Height = Height;
	SwapChainDesc.SampleDesc = { 1, 0 };
	SwapChainDesc.Flags = 0;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;;
	
	Microsoft::WRL::ComPtr<IDXGISwapChain1> TempSwapChain;
	if (FAILED(Device->GetFactory()->CreateSwapChainForHwnd(CommandQueue->GetNativeCommandQueue(), Hwnd, &SwapChainDesc, nullptr, nullptr, TempSwapChain.GetAddressOf())))
	{
		return false;
	}
	TempSwapChain.As(&SwapChain);

	D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc{};
	RTVHeapDesc.NumDescriptors = BufferCount;
	RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	RTVHeapDesc.NodeMask = 0;

	if (FAILED(Device->GetDevice()->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&RTVHeap))))
	{
		return false;
	}
	RTVDescriptorSize = Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = RTVHeap->GetCPUDescriptorHandleForHeapStart();

	for (int i = 0; i < BufferCount; i++)
	{
		if (FAILED(SwapChain->GetBuffer(i, IID_PPV_ARGS(&BackBuffers[i]))))
		{
			return false;
		}
		
		Device->GetDevice()->CreateRenderTargetView(BackBuffers[i], nullptr, RTVHandle);
		RTVHandle.ptr += RTVDescriptorSize;
	}

	return true;
}

void D3D12SwapChain::Present()
{
	SwapChain->Present(1, 0);
}

UINT32 D3D12SwapChain::GetBackBufferIndex() const
{
	UINT CurrentIndex = SwapChain->GetCurrentBackBufferIndex();
	return CurrentIndex;
}

ID3D12Resource* D3D12SwapChain::GetCurrentBackBuffer() const
{
	UINT Index = GetBackBufferIndex();
	return BackBuffers[Index];
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12SwapChain::GetCurrentRTV() const
{
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = RTVHeap->GetCPUDescriptorHandleForHeapStart();
	RTVHandle.ptr += (GetBackBufferIndex() * RTVDescriptorSize);

	return RTVHandle;
}
