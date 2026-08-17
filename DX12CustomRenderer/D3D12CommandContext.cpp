#include "D3D12CommandContext.h"

D3D12CommandContext::~D3D12CommandContext()
{
	if (CommandList)
	{
		CommandList->Release();
		CommandList = nullptr;
	}
}

bool D3D12CommandContext::Initialize(D3D12Device* Device, ID3D12CommandAllocator* CommandAllocator)
{
	if(!Device || !Device->GetDevice())
	{
		return false;
	}

	if (FAILED(Device->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocator, nullptr, IID_PPV_ARGS(&CommandList))))
	{
		return false;
	}
	//CommandList는 생성 직후 Open(Recording) 상태이므로 Close()를 호출하여 초기화
	if (FAILED(CommandList->Close()))
	{
		return false;
	}

	return true;
}

bool D3D12CommandContext::Reset(ID3D12CommandAllocator* CommandAllocator)
{
	if (!CommandAllocator || !CommandList)
	{
		return false;
	}
	if (FAILED(CommandAllocator->Reset()))
	{
		return false;
	}
	if (FAILED(CommandList->Reset(CommandAllocator, nullptr)))
	{
		return false;
	}
	return true;
}

bool D3D12CommandContext::Close()
{
	if (FAILED(CommandList->Close()))
	{
		return false;
	}
	return true;
}