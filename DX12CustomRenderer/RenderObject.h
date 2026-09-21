#pragma once

#include "RenderModel.h"

#include <DirectXMath.h>
#include <memory>

struct RenderObject
{
    std::shared_ptr<RenderModel> Model;
    DirectX::XMFLOAT4X4 World;
};