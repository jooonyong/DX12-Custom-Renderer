#pragma once

#include <string>
#include "cgltf.h"
#include "ModelData.h"

class GLTFLoader
{
public:
    static bool Load(const std::string& FilePath, ModelData& OutModel);
    static bool LoadImage(const std::string& FilePath, const cgltf_image* Image, ImageData& Data);

    static bool GenerateTangent(MeshData& Mesh);
};