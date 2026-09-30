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

void RenderGraph::Compile()
{
	BuildDependency();
}

void RenderGraph::BuildDependency()
{
	if (Passes.size() > 1)
	{
		for (uint32_t i = 1; i < Passes.size(); i++)
		{
			for (uint32_t j = 0; j < i; j++)
			{
				if (HasDependency(Passes[j], Passes[i]))
				{
					Passes[i].Dependencies.push_back(j);
				}
			}
		}
	}
}

bool RenderGraph::HasDependency(RenderGraphPass& PrevPass, RenderGraphPass& CurrentPass)
{
	for (auto& PrevUsage : PrevPass.ResourceUsages)
	{
		//Usage : Resource, ResourceState, RenderAccess
		for (auto& CurrentUsage : CurrentPass.ResourceUsages)
		{
			if (CurrentUsage.Resource != PrevUsage.Resource)
			{
				continue;
			}
			if (CurrentUsage.Access == RenderGraphAccess::Read && PrevUsage.Access == RenderGraphAccess::Read)
			{
				continue;
			}
			return true;
		}
	}
	return false;
}
