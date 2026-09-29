#include "D3D12ResourceStateTracker.h"

void ResourceStateTracker::RegisterResource(ID3D12Resource* Resource, D3D12_RESOURCE_STATES InitialState)
{
	ResourceStates[Resource] = InitialState;
}

void ResourceStateTracker::Transition(ID3D12GraphicsCommandList* CommandList, ID3D12Resource* Resource, D3D12_RESOURCE_STATES NewState)
{
	if (ResourceStates[Resource] == NewState)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER ResourceBarrier{};
	ResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	ResourceBarrier.Transition.pResource = Resource;
	ResourceBarrier.Transition.StateBefore = ResourceStates[Resource];
	ResourceBarrier.Transition.StateAfter = NewState;
	ResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandList->ResourceBarrier(1, &ResourceBarrier);
	
	ResourceStates[Resource] = NewState;
}

D3D12_RESOURCE_STATES ResourceStateTracker::GetResourceState(ID3D12Resource* Resource)
{
	return ResourceStates[Resource];
}
