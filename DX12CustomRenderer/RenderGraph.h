#pragma once

#include <string>
#include <vector>
#include <functional>

#include "d3d12.h"
#include "D3D12ResourceStateTracker.h"

enum class RenderGraphAccess
{
	Read,
	Write,
	ReadWrite
};

struct RenderGraphResourceUsage
{
	ID3D12Resource* Resource = nullptr;
	D3D12_RESOURCE_STATES RequiredState = D3D12_RESOURCE_STATE_COMMON;
	RenderGraphAccess Access = RenderGraphAccess::Read;
};

struct RenderGraphPass
{
	std::string Name;
	std::vector<RenderGraphResourceUsage> ResourceUsages;
	std::vector<uint32_t> Dependencies;
	std::function<void(ID3D12GraphicsCommandList*)> Execute;
};

class RenderGraph
{
public:
    void AddPass(std::string Name, std::vector<RenderGraphResourceUsage> Resources, std::function<void(ID3D12GraphicsCommandList*)> Execute);
    void Execute(ID3D12GraphicsCommandList* CommandList, ResourceStateTracker& StateTracker);

	void Compile();
	void BuildDependency();
	void BuildExecutionOrder();
	void Reset();

	bool HasDependency(RenderGraphPass& PrevPass, RenderGraphPass& CurrentPass);

private:
	std::vector<RenderGraphPass> Passes;
	std::vector<uint32_t> ExecutionOrder;
};