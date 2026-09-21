#pragma once
#include "RenderObject.h"

class Scene
{
public:
    void AddRenderObject(const RenderObject& Object);
    const std::vector<RenderObject>& GetRenderObjects() const;

private:
    std::vector<RenderObject> RenderObjects;
};