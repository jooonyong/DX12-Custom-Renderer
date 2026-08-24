#include "D3D12ResourceUploader.h"
#include "D3D12CommandContext.h"

bool D3D12ResourceUploader::Initialize(D3D12Device* InDevice, D3D12CommandQueue* InCommandQueue, D3D12CommandContext* InCommandContext, ID3D12CommandAllocator* InCommandAllocator)
{
	Device = InDevice;
	CommandQueue = InCommandQueue;
	CommandContext = InCommandContext;
	CommandAllocator = std::move(InCommandAllocator);

	return true;
}

bool D3D12ResourceUploader::Begin()
{
	if (!CommandContext || !CommandAllocator)
	{
		return false;
	}

	CommandContext->Reset(CommandAllocator);
	PendingUploadBuffers.clear();

	return true;
}

bool D3D12ResourceUploader::UploadBuffer(const void* Data, UINT64 Size, D3D12_RESOURCE_STATES FinalState, Microsoft::WRL::ComPtr<ID3D12Resource>& OutResource)
{
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadBuffer;
	D3D12_HEAP_PROPERTIES UploadHeapProp{};
	UploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	
	D3D12_RESOURCE_DESC BufferDesc{};
	BufferDesc.Width = Size;
	BufferDesc.Height = 1;
	BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	BufferDesc.DepthOrArraySize = 1;
	BufferDesc.MipLevels = 1;
	BufferDesc.SampleDesc.Count = 1;
	BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	if (FAILED(Device->GetDevice()->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc, 
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&UploadBuffer))))
	{
		return false;
	}

	void* MappedData = nullptr;
	if (FAILED(UploadBuffer->Map(0, nullptr, &MappedData)))
	{
		return false;
	}
	//배열 데이터를 해당 주소에 복사
	memcpy(MappedData, Data, Size);

	UploadBuffer->Unmap(0, nullptr);

	D3D12_HEAP_PROPERTIES DefaultHeapProperties{};
	DefaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	if (FAILED(Device->GetDevice()->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE,
		&BufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(OutResource.ReleaseAndGetAddressOf()))))
	{
		return false;
	}

	CommandContext->GetCommandList()->CopyBufferRegion(OutResource.Get(), 0, UploadBuffer.Get(), 0, Size);

	D3D12_RESOURCE_BARRIER ResourceBarrier{};
	ResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	ResourceBarrier.Transition.pResource = OutResource.Get();
	ResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	ResourceBarrier.Transition.StateAfter = FinalState;
	ResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandContext->GetCommandList()->ResourceBarrier(1, &ResourceBarrier);

	PendingUploadBuffers.push_back(std::move(UploadBuffer));

	return true;
}

bool D3D12ResourceUploader::End()
{
	CommandContext->Close();

	CommandQueue->Execute(CommandContext);

	UINT64 FenceValue = CommandQueue->Signal();
	if (FenceValue == 0)
	{
		return false;
	}

	CommandQueue->WaitForFence(FenceValue);
	PendingUploadBuffers.clear();

	return true;
}