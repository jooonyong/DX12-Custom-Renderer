#pragma once

#include "MeshData.h"

struct MaterialData
{
	DirectX::XMFLOAT4 BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	float Roughness = 0.3f;
	float Metallic = 0.0f;
};

struct ModelData
{
	MeshData Mesh;
	MaterialData Material;
};