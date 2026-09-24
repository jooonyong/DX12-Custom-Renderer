#pragma once

#include <d3d12.h>
#include <wrl.h>
#include "D3D12CommandContext.h"

struct D3D12DescriptorHandle
{
    UINT Index = UINT_MAX;
    UINT Count = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE CPU{};
    D3D12_GPU_DESCRIPTOR_HANDLE GPU{};

    bool IsValid() const
    {
        return Index != UINT_MAX;
    }
};

class D3D12DescriptorAllocator
{
public:
	bool Initialize(D3D12Device* Device, UINT Capacity);

    D3D12DescriptorHandle Allocate(UINT Count);

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(D3D12DescriptorHandle& DescriptorHandle, UINT Offset);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(D3D12DescriptorHandle& DescriptorHandle, UINT Offset);

    ID3D12DescriptorHeap* GetHeap() const { return DescriptorHeap.Get(); }
private:
    D3D12Device* Device = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DescriptorHeap;

    UINT DescriptorSize = 0;
    UINT Capacity = 0;
    UINT NextFreeIndex = 0;
};