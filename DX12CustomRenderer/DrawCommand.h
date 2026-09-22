#pragma once

#include <cstdint>

class Mesh;
class Material;

struct DrawCommand
{
	Mesh* Mesh = nullptr;
	Material* Material = nullptr;

	uint32_t IndexStart = 0;
	uint32_t IndexCount = 0;
	uint32_t ObjectIndex = 0;
};