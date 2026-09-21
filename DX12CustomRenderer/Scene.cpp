#include "Scene.h"

void Scene::AddRenderObject(const RenderObject& Object)
{
	RenderObjects.push_back(Object);
}

const std::vector<RenderObject>& Scene::GetRenderObjects() const
{
	// TODO: insert return statement here
	return RenderObjects;
}
