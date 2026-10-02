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
	if (RaytracingDevice)
	{
		RaytracingDevice->Release();
		RaytracingDevice = nullptr;
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
	if (!CheckRaytracingSupport())
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
	if (FAILED(Device->QueryInterface(IID_PPV_ARGS(&RaytracingDevice))))
	{
		return false;
	}
	return true;
}

bool D3D12Device::CheckRaytracingSupport()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 Options5{};
	HRESULT Result = RaytracingDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &Options5, sizeof(Options5));
	if (FAILED(Result))
	{
		return false;
	}
	if (Options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
	{
		return false;
	}
	return true;
}
