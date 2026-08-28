#pragma once

#include "MeshData.h"

#define PI 3.141592

namespace GeometryGenerator
{
    MeshData CreateCube();
    MeshData CreateSphere(float Radius, uint32_t SliceCount, uint32_t StackCount);
}