#pragma once

#include "RenderModel.h"

#include <DirectXMath.h>
#include <memory>

struct RenderObject
{
    uint64_t ObjectID;
    std::shared_ptr<RenderModel> Model;
    DirectX::XMFLOAT4X4 World;

};