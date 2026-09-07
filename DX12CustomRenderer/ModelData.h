#pragma once

#include "MeshData.h"
#include "ImageData.h"
#include <optional>
#include <cstdint>
#include <limits>
#include <vector>

constexpr uint32_t InvalidMaterialIndex = (std::numeric_limits<uint32_t>::max)();

struct SubMeshData
{
	uint32_t IndexStart = 0;
	uint32_t IndexCount = 0;
	uint32_t MaterialIndex = InvalidMaterialIndex;
};

struct MaterialData
{
	DirectX::XMFLOAT4 BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	float Roughness = 0.3f;
	float Metallic = 0.0f;
	
	std::optional<ImageData> BaseColorImage;
	std::optional<ImageData> MetallicRoughnessImage;
	std::optional<ImageData> NormalMapImage;
};

struct ModelData
{
	MeshData Mesh;
	std::vector<SubMeshData> SubMeshes;
	std::vector<MaterialData> Materials;
};