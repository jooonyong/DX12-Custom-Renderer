#pragma once

#include <vector>
#include "DirectXMath.h"

struct Vertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT4 Color;
	DirectX::XMFLOAT2 UV;
};

struct MeshData
{
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;
};