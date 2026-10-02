#pragma once
#include "d3d12.h"
#include "dxgi1_6.h"
#include <wrl.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class D3D12Device
{
public:
	~D3D12Device();

	bool Initialize();

	ID3D12Device* GetDevice() const { return Device; }
	IDXGIFactory4* GetFactory() const { return Factory; }
	ID3D12Device5* GetRaytracingDevice() const { return RaytracingDevice; }

private:
	bool CreateFactory();
	bool SelectAdapter();
	bool CreateDevice();
	bool CheckRaytracingSupport();

private:
	IDXGIAdapter* Adapter = nullptr;;
	IDXGIFactory7* Factory = nullptr;
	ID3D12Device* Device = nullptr;
	ID3D12Device5* RaytracingDevice = nullptr;
};