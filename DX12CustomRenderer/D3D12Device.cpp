#include "D3D12Device.h"

D3D12Device::~D3D12Device()
{
	if (Factory)
	{
		Factory->Release();
		Factory = nullptr;
	}
	if (Adapter)
	{
		Adapter->Release();
		Adapter = nullptr;
	}
	if (Device)
	{
		Device->Release();
		Device = nullptr;
	}
}

bool D3D12Device::Initialize()
{
	//D3D12 Debug¿ë
	Microsoft::WRL::ComPtr<ID3D12Debug> DebugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(DebugController.GetAddressOf()))))
	{
		DebugController->EnableDebugLayer();
	}


	if (!CreateFactory())
	{
		return false;
	}
	if (!SelectAdapter())
	{
		return false;
	}
	if (!CreateDevice())
	{
		return false;
	}
	return true;
}

bool D3D12Device::CreateFactory()
{
	if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&Factory))))
	{
		return false;
	}
	return true;
}

bool D3D12Device::SelectAdapter()
{
	if (Factory)
	{
		if (FAILED(Factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&Adapter))))
		{
			return false;
		}
	}
	return true;
}

bool D3D12Device::CreateDevice()
{
	if (FAILED(D3D12CreateDevice(Adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&Device))))
	{
		return false;
	}
	return true;
}
