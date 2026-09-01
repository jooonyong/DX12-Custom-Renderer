#pragma once

#include "MeshData.h"
#include "ImageData.h"
#include <optional>

struct MaterialData
{
	DirectX::XMFLOAT4 BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	float Roughness = 0.3f;
	float Metallic = 0.0f;
	
	std::optional<ImageData> BaseColorImage;
};

struct ModelData
{
	MeshData Mesh;
	MaterialData Material;
};