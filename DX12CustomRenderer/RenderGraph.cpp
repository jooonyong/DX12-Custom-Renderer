#include "RenderGraph.h"

void RenderGraph::AddPass(std::string Name, std::vector<RenderGraphResourceUsage> Resources, std::function<void(ID3D12GraphicsCommandList*)> Execute)
{
	RenderGraphPass Pass{};
	Pass.Name = Name;
	Pass.ResourceUsages = Resources;
	Pass.Execute = Execute;

	Passes.push_back(Pass);
}

void RenderGraph::Execute(ID3D12GraphicsCommandList* CommandList, ResourceStateTracker& StateTracker)
{
	for (auto& Pass : Passes)
	{
		for (const auto& Usage : Pass.ResourceUsages)
		{
			StateTracker.Transition(CommandList, Usage.Resource, Usage.RequiredState);
		}
		Pass.Execute(CommandList);
	}
}

void RenderGraph::Reset()
{
	Passes.clear();
}