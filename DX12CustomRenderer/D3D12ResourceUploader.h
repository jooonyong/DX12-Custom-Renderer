#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>

class D3D12Device;
class D3D12CommandQueue;
class D3D12CommandContext;

class D3D12ResourceUploader
{
public:
    bool Initialize(D3D12Device* InDevice, D3D12CommandQueue* InCommandQueue, D3D12CommandContext* InCommandContext,ID3D12CommandAllocator* InCommandAllocator);
    bool Begin();
    bool UploadBuffer(const void* Data, UINT64 Size, D3D12_RESOURCE_STATES FinalState, Microsoft::WRL::ComPtr<ID3D12Resource>& OutResource);
    bool End();

private:
    D3D12Device* Device = nullptr;
    D3D12CommandQueue* CommandQueue = nullptr;
    D3D12CommandContext* CommandContext = nullptr;
    ID3D12CommandAllocator* CommandAllocator = nullptr;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> PendingUploadBuffers;
};