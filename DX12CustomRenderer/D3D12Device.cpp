#include "D3D12Device.h"

bool D3D12Device::Initialize()
{
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
	if (FAILED(D3D12CreateDevice(Adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&Device))))
	{
		return false;
	}
	return true;
}
