#include "D3D12CommandQueue.h"
#include "D3D12CommandContext.h"

D3D12CommandQueue::~D3D12CommandQueue()
{
	if (FenceEvent)
	{
		CloseHandle(FenceEvent);
		FenceEvent = nullptr;
	}
	if (CommandQueue)
	{
		CommandQueue->Release();
		CommandQueue = nullptr;
	}
	if (Fence)
	{
		Fence->Release();
		Fence = nullptr;
	}

}

bool D3D12CommandQueue::Initialize(D3D12Device* Device)
{
	if (!Device)
	{
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
	QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	QueueDesc.NodeMask = 0;
	QueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	if(FAILED(Device->GetDevice()->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue))))
	{
		return false;
	}
	if (FAILED(Device->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence))))
	{
		return false;
	}
	FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	return true;
}

void D3D12CommandQueue::Execute(D3D12CommandContext* Command)
{
	if (!Command || !Command->GetCommandList())
	{
		return;
	}
	ID3D12CommandList* CommandLists[] = { Command->GetCommandList() };
	CommandQueue->ExecuteCommandLists(1, CommandLists);
}

ID3D12CommandQueue* D3D12CommandQueue::GetNativeCommandQueue() const
{
	return CommandQueue;
}

UINT64 D3D12CommandQueue::Signal()
{
	FenceValue++;
	if (FAILED(CommandQueue->Signal(Fence, FenceValue)))
	{
		return 0;
	}
	return FenceValue;
}

UINT64 D3D12CommandQueue::GetFenceValue()
{
	return FenceValue;
}

void D3D12CommandQueue::WaitForFence(UINT64 FenceValue)
{
	if (Fence->GetCompletedValue() < FenceValue)
	{
		if (FAILED(Fence->SetEventOnCompletion(FenceValue, FenceEvent)))
		{
			return;
		}
		WaitForSingleObject(FenceEvent, INFINITE);
	}
}

void D3D12CommandQueue::WaitForIdle()
{
	UINT64 CurrentFenceValue = Signal();
	WaitForFence(CurrentFenceValue);
}
