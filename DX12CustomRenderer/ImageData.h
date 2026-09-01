#pragma once

#include <cstdint>
#include <vector>

struct ImageData
{
	uint32_t Width;
	uint32_t Height;
	std::vector<uint8_t> Pixels;
};