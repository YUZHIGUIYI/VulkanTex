#pragma once

#include "Tex.h"

//=====================================================================================
// Bitmask flags enumerator operators
//=====================================================================================
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace TexBackend
{
    inline bool IsCompressed(VkFormat fmt) noexcept
    {
        switch (fmt)
        {
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            case VK_FORMAT_BC2_UNORM_BLOCK:
            case VK_FORMAT_BC2_SRGB_BLOCK:
            case VK_FORMAT_BC3_UNORM_BLOCK:
            case VK_FORMAT_BC3_SRGB_BLOCK:
            case VK_FORMAT_BC4_UNORM_BLOCK:
            case VK_FORMAT_BC4_SNORM_BLOCK:
            case VK_FORMAT_BC5_UNORM_BLOCK:
            case VK_FORMAT_BC5_SNORM_BLOCK:
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:
            case VK_FORMAT_BC6H_SFLOAT_BLOCK:
            case VK_FORMAT_BC7_UNORM_BLOCK:
            case VK_FORMAT_BC7_SRGB_BLOCK:
                return true;

            default:
                return false;
        }
    }


    inline bool IsSRGB(VkFormat fmt) noexcept
    {
        switch (fmt)
        {
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_BC7_SRGB_BLOCK:
            return true;

        default:
            return false;
        }
    }

    //=====================================================================================
    // Image I/O
    //=====================================================================================

    inline bool SaveToDDSMemory(const Image& image, DDS_FLAGS flags, Blob& blob) noexcept
    {
        TexMetadata mdata = {};
        mdata.width = image.width;
        mdata.height = image.height;
        mdata.depth = 1;
        mdata.arraySize = 1;
        mdata.mipLevels = 1;
        mdata.format = image.format;
        mdata.dimension = TEX_DIMENSION_TEXTURE2D;

        return SaveToDDSMemory(&image, 1, mdata, flags, blob);
    }


    inline bool SaveToDDSFile(const Image& image, DDS_FLAGS flags, const wchar_t* szFile) noexcept
    {
        TexMetadata mdata = {};
        mdata.width = image.width;
        mdata.height = image.height;
        mdata.depth = 1;
        mdata.arraySize = 1;
        mdata.mipLevels = 1;
        mdata.format = image.format;
        mdata.dimension = TEX_DIMENSION_TEXTURE2D;

        return SaveToDDSFile(&image, 1, mdata, flags, szFile);
    }


    //=====================================================================================
    // Compatability helpers
    //=====================================================================================
    inline bool GetMetadataFromTGAMemory(const uint8_t* pSource, size_t size, TexMetadata& metadata) noexcept
    {
        return GetMetadataFromTGAMemory(pSource, size, TGA_FLAGS_NONE, metadata);
    }


    inline bool GetMetadataFromTGAFile(const wchar_t* szFile, TexMetadata& metadata) noexcept
    {
        return GetMetadataFromTGAFile(szFile, TGA_FLAGS_NONE, metadata);
    }


    inline bool LoadFromTGAMemory(const uint8_t* pSource, size_t size, TexMetadata* metadata, ScratchImage& image) noexcept
    {
        return LoadFromTGAMemory(pSource, size, TGA_FLAGS_NONE, metadata, image);
    }


    inline bool LoadFromTGAFile(const wchar_t* szFile, TexMetadata* metadata, ScratchImage& image) noexcept
    {
        return LoadFromTGAFile(szFile, TGA_FLAGS_NONE, metadata, image);
    }


    inline bool SaveToTGAMemory(const Image& image, Blob& blob, const TexMetadata* metadata) noexcept
    {
        return SaveToTGAMemory(image, TGA_FLAGS_NONE, blob, metadata);
    }


    inline bool SaveToTGAFile(const Image& image, const wchar_t* szFile, const TexMetadata* metadata) noexcept
    {
        return SaveToTGAFile(image, TGA_FLAGS_NONE, szFile, metadata);
    }


    //=====================================================================================
    // C++17 helpers
    //=====================================================================================
#ifdef __cpp_lib_byte
    inline bool GetMetadataFromDDSMemory(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata& metadata) noexcept
    {
        return GetMetadataFromDDSMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata);
    }


    inline bool LoadFromDDSMemory(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata* metadata, ScratchImage& image) noexcept
    {
        return LoadFromDDSMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, image);
    }


    inline bool GetMetadataFromDDSMemoryEx(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata& metadata, DDSMetaData* ddPixelFormat) noexcept
    {
        return GetMetadataFromDDSMemoryEx(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, ddPixelFormat);
    }


    inline bool LoadFromDDSMemoryEx(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata* metadata, DDSMetaData* ddPixelFormat, ScratchImage& image) noexcept
    {
        return LoadFromDDSMemoryEx(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, ddPixelFormat, image);
    }


    inline bool GetMetadataFromHDRMemory(const std::byte* pSource, size_t size, TexMetadata& metadata) noexcept
    {
        return GetMetadataFromHDRMemory(reinterpret_cast<const uint8_t*>(pSource), size, metadata);
    }


    inline bool LoadFromHDRMemory(const std::byte* pSource, size_t size, TexMetadata* metadata, ScratchImage& image) noexcept
    {
        return LoadFromHDRMemory(reinterpret_cast<const uint8_t*>(pSource), size, metadata, image);
    }


    inline bool GetMetadataFromTGAMemory(const std::byte* pSource, size_t size, TGA_FLAGS flags, TexMetadata& metadata) noexcept
    {
        return GetMetadataFromTGAMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata);
    }


    inline bool LoadFromTGAMemory(const std::byte* pSource, size_t size, TGA_FLAGS flags, TexMetadata* metadata, ScratchImage& image) noexcept
    {
        return LoadFromTGAMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, image);
    }


    inline bool EncodeDDSHeader(const TexMetadata& metadata, DDS_FLAGS flags, std::byte* pDestination, size_t maxsize, size_t& required) noexcept
    {
        return EncodeDDSHeader(metadata, flags, reinterpret_cast<uint8_t*>(pDestination), maxsize, required);
    }


    inline bool EncodeDDSHeader(const TexMetadata& metadata, DDS_FLAGS flags, std::nullptr_t, size_t maxsize, size_t& required) noexcept
    {
        return EncodeDDSHeader(metadata, flags, static_cast<uint8_t*>(nullptr), maxsize, required);
    }
#endif // __cpp_lib_byte
}

