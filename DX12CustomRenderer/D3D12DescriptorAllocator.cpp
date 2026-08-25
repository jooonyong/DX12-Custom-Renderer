#include "D3D12DescriptorAllocator.h"

bool D3D12DescriptorAllocator::Initialize(D3D12Device* Device, UINT Capacity)
{
	this->Device = Device;
	this->Capacity = Capacity;
	NextFreeIndex = 0;

	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc{};
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HeapDesc.NumDescriptors = this->Capacity;
	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HeapDesc.NodeMask = 0;

	HRESULT Result = Device->GetDevice()->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&DescriptorHeap));
	if (FAILED(Result))
	{
		return false;
	}

	this->DescriptorSize = Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return true;
}

D3D12DescriptorHandle D3D12DescriptorAllocator::Allocate()
{
	D3D12DescriptorHandle Handle{};
	if (NextFreeIndex >= Capacity)
	{
		return Handle;
	}
	
	D3D12_CPU_DESCRIPTOR_HANDLE CPUStart = DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE GPUStart = DescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	Handle.Index = NextFreeIndex;
	Handle.CPU.ptr = CPUStart.ptr + static_cast<SIZE_T>(NextFreeIndex) * DescriptorSize;
	Handle.GPU.ptr = GPUStart.ptr + static_cast<SIZE_T>(NextFreeIndex) * DescriptorSize;

	NextFreeIndex++;

	return Handle;
}