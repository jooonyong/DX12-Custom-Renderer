#pragma once
#include "d3d12.h"
#include <unordered_map>

class ResourceStateTracker
{
public:
    void RegisterResource(ID3D12Resource* Resource, D3D12_RESOURCE_STATES InitialState);
    void Transition(ID3D12GraphicsCommandList* CommandList, ID3D12Resource* Resource, D3D12_RESOURCE_STATES NewState);

    D3D12_RESOURCE_STATES GetResourceState(ID3D12Resource* Resource);

private:
    std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES> ResourceStates;
};