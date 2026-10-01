#include "RenderGraph.h"
#include <algorithm>
#include <queue>
#include <cassert>

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
	for (auto& Order : ExecutionOrder)
	{
		RenderGraphPass& Pass = Passes[Order];
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
	BuildExecutionOrder();
}

void RenderGraph::BuildDependency()
{
	for (auto& Pass : Passes)
	{
		Pass.Dependencies.clear();
	}
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

void RenderGraph::BuildExecutionOrder()
{
	//Kahn Algorithm
	ExecutionOrder.clear();

	std::vector<uint32_t> InDegrees(Passes.size(), 0);
	std::queue<uint32_t> q;

	for (int i = 0; i < Passes.size(); i++)
	{
		InDegrees[i] = static_cast<uint32_t>(Passes[i].Dependencies.size());
		if (Passes[i].Dependencies.size() == 0)
		{
			q.push(i);
		}
	}
	while (!q.empty())
	{
		uint32_t CurrentIndex = q.front();
		q.pop();
		ExecutionOrder.push_back(CurrentIndex);

		for (int i = 0; i < Passes.size(); i++)
		{
			for (int j = 0; j < Passes[i].Dependencies.size(); j++)
			{
				if (Passes[i].Dependencies[j] != CurrentIndex)
				{
					continue;
				}
				InDegrees[i]--;
				if (InDegrees[i] == 0)
				{
					q.push(i);
				}
				break;
			}
		}
	}
	if (ExecutionOrder.size() != Passes.size())
	{
		assert(false);
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
