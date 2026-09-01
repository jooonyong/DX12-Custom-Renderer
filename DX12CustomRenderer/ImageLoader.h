#pragma once

#include <string>
#include "ImageData.h"

class ImageLoader
{
public:
    static bool LoadFromFile(const std::wstring& FilePath, ImageData& OutImage);
    static bool LoadFromMemory(const uint8_t* Data, size_t Size,ImageData& OutImage);
};