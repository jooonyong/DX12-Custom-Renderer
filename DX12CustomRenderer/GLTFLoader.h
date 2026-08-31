#pragma once

#include <string>
#include "ModelData.h"

class GLTFLoader
{
public:
    static bool Load(const std::string& FilePath, ModelData& OutModel);
};