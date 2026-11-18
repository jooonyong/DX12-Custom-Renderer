#include "Scene.h"
#include <atomic>

uint64_t Scene::AddRenderObject(const RenderObject& Object)
{
	static std::atomic<uint64_t> NextObjectID = 1;
	RenderObject NewObject = Object;
	NewObject.ObjectID = NextObjectID.fetch_add(1, std::memory_order_relaxed);
	const uint64_t AssignedID = NewObject.ObjectID;
	RenderObjects.push_back(std::move(NewObject));

	return AssignedID;
}

const std::vector<RenderObject>& Scene::GetRenderObjects() const
{
	// TODO: insert return statement here
	return RenderObjects;
}
