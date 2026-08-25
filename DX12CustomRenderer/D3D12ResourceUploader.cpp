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

bool D3D12ResourceUploader::UploadTexture(const uint8_t* Pixels, UINT Width, UINT Height, D3D12_RESOURCE_STATES FinalState, Microsoft::WRL::ComPtr<ID3D12Resource>& OutTexture)
{
	D3D12_RESOURCE_DESC TextureDesc{};
	TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TextureDesc.MipLevels = 1;
	TextureDesc.Alignment = 0;
	TextureDesc.Width = Width;
	TextureDesc.Height = Height;
	TextureDesc.DepthOrArraySize = 1;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	TextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES DefaultHeapProperties{};
	DefaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT Result = Device->GetDevice()->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &TextureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(OutTexture.ReleaseAndGetAddressOf()));
	if (FAILED(Result))
	{
		return false;
	}

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT FootPrint{};
	UINT NumRow;
	UINT64 RowSize;
	UINT64 UploadBufferSize;
	Device->GetDevice()->GetCopyableFootprints(&TextureDesc, 0, 1, 0, &FootPrint, &NumRow, &RowSize, &UploadBufferSize);

	Microsoft::WRL::ComPtr<ID3D12Resource> TextureUploadBuffer = nullptr;

	D3D12_RESOURCE_DESC UploadDesc{};
	UploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	UploadDesc.Width = UploadBufferSize;
	UploadDesc.Height = 1;
	UploadDesc.MipLevels = 1;
	UploadDesc.DepthOrArraySize = 1;
	UploadDesc.SampleDesc.Count = 1;
	UploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	D3D12_HEAP_PROPERTIES UploadHeapProperties{};
	UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	Result = Device->GetDevice()->CreateCommittedResource(&UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &UploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&TextureUploadBuffer));
	if (FAILED(Result))
	{
		return false;
	}

	uint8_t* MappedData = nullptr;
	TextureUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&MappedData));
	for (int i = 0; i < NumRow; i++)
	{
		const uint8_t* Src = Pixels + i * RowSize;
		uint8_t* Dest = MappedData + FootPrint.Offset + i * FootPrint.Footprint.RowPitch;

		memcpy(Dest, Src, RowSize);
	}

	D3D12_TEXTURE_COPY_LOCATION Dst{};
	Dst.pResource = OutTexture.Get();
	Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	Dst.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION Src{};
	Src.pResource = TextureUploadBuffer.Get();
	Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	Src.PlacedFootprint = FootPrint;

	TextureUploadBuffer->Unmap(0, nullptr);

	CommandContext->GetCommandList()->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);

	D3D12_RESOURCE_BARRIER Barrier{};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = OutTexture.Get();
	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandContext->GetCommandList()->ResourceBarrier(1, &Barrier);

	PendingUploadBuffers.push_back(std::move(TextureUploadBuffer));

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