#pragma once

#include "Mesh.h"
#include "Material.h"
#include "ModelData.h"

#include <memory>
#include <vector>

struct RenderModel
{
    std::unique_ptr<Mesh> Mesh;
    std::vector<SubMeshData> SubMeshes;
    std::vector<std::shared_ptr<Material>> Materials;
};