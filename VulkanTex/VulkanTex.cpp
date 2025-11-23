#include <cstdlib>
#include <vulkan/vulkan_core.h>
#include "VulkanTex.h"

namespace VulkanTex
{
    //=====================================================================================
    // Vulkan Format Utilities
    //=====================================================================================
    bool IsValid(VkFormat fmt) noexcept
    {
        return (static_cast<size_t>(fmt) >= 1 && static_cast<size_t>(fmt) <= 1000609013);
    }

    static constexpr bool ispow2(size_t x) noexcept
    {
        return ((x != 0) && !(x & (x - 1)));
    }

    static constexpr size_t CountMips(size_t width, size_t height) noexcept
    {
        size_t mipLevels = 1;

        while (height > 1 || width > 1)
        {
            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;

            ++mipLevels;
        }

        return mipLevels;
    }

    static constexpr size_t CountMips3D(size_t width, size_t height, size_t depth) noexcept
    {
        size_t mipLevels = 1;

        while (height > 1 || width > 1 || depth > 1)
        {
            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;

            if (depth > 1)
                depth >>= 1;

            ++mipLevels;
        }

        return mipLevels;
    }

    bool CalculateMipLevels(
        size_t width,
        size_t height,
        size_t& mipLevels) noexcept
    {
        if (mipLevels > 1)
        {
            const size_t maxMips = CountMips(width, height);

            if (mipLevels > maxMips)
                return false;
        }
        else if (mipLevels == 0)
        {
            mipLevels = CountMips(width, height);
        }
        else
        {
            mipLevels = 1;
        }

        return true;
    }

    bool CalculateMipLevels3D(
        size_t width,
        size_t height,
        size_t depth,
        size_t& mipLevels) noexcept
    {
        if (mipLevels > 1)
        {
            const size_t maxMips = CountMips3D(width, height, depth);

            if (mipLevels > maxMips)
                return false;
        }
        else if (mipLevels == 0)
        {
            mipLevels = CountMips3D(width, height, depth);
        }
        else
        {
            mipLevels = 1;
        }
        return true;
    }

    //-------------------------------------------------------------------------------------
    // Determines number of image array entries and pixel size
    //-------------------------------------------------------------------------------------
    bool DetermineImageArray(
        const TexMetadata& metadata,
        CP_FLAGS cpFlags,
        size_t& nImages,
        size_t& pixelSize) noexcept
    {
        assert(metadata.width > 0 && metadata.height > 0 && metadata.depth > 0);
        assert(metadata.arraySize > 0);
        assert(metadata.mipLevels > 0);

        uint64_t totalPixelSize = 0;
        size_t nimages = 0;

        switch (metadata.dimension)
        {
            case TEX_DIMENSION_TEXTURE1D:
            case TEX_DIMENSION_TEXTURE2D:
            {
                for (size_t item = 0; item < metadata.arraySize; ++item)
                {
                    size_t w = metadata.width;
                    size_t h = metadata.height;

                    for (size_t level = 0; level < metadata.mipLevels; ++level)
                    {
                        size_t rowPitch, slicePitch;
                        bool hr = ComputePitch(metadata.format, w, h, rowPitch, slicePitch, cpFlags);

                        if (hr == false)
                        {
                            nImages = pixelSize = 0;
                            return hr;
                        }

                        totalPixelSize += uint64_t(slicePitch);
                        ++nimages;

                        if (h > 1)
                            h >>= 1;

                        if (w > 1)
                            w >>= 1;
                    }
                }
                break;
            }

            case TEX_DIMENSION_TEXTURE3D:
            {
                size_t w = metadata.width;
                size_t h = metadata.height;
                size_t d = metadata.depth;

                for (size_t level = 0; level < metadata.mipLevels; ++level)
                {
                    size_t rowPitch, slicePitch;
                    bool hr = ComputePitch(metadata.format, w, h, rowPitch, slicePitch, cpFlags);

                    if (hr == false)
                    {
                        nImages = pixelSize = 0;
                        return hr;
                    }

                    for (size_t slice = 0; slice < d; ++slice)
                    {
                        totalPixelSize += uint64_t(slicePitch);
                        ++nimages;
                    }

                    if (h > 1)
                        h >>= 1;

                    if (w > 1)
                        w >>= 1;

                    if (d > 1)
                        d >>= 1;
                }

                break;
            }            

            default:
                nImages = pixelSize = 0;
                return false;
        }

#if defined(_M_IX86) || defined(_M_ARM) || defined(_M_HYBRID_X86_ARM64)
        static_assert(sizeof(size_t) == 4, "Not a 32-bit platform!");
        if (totalPixelSize > UINT32_MAX)
        {
            nImages = pixelSize = 0;
            return false;
        }
#else
        static_assert(sizeof(size_t) == 8, "Not a 64-bit platform!");

        if ((cpFlags & CP_FLAGS_LIMIT_4GB) && (totalPixelSize > UINT32_MAX))
        {
            nImages = pixelSize = 0;
            return false;
        }
#endif

        nImages   = nimages;
        pixelSize = static_cast<size_t>(totalPixelSize);

        return true;
    }

    //-------------------------------------------------------------------------------------
    // Fills in the image array entries
    //-------------------------------------------------------------------------------------
    bool SetupImageArray(
        uint8_t *pMemory,
        size_t pixelSize,
        const TexMetadata& metadata,
        CP_FLAGS cpFlags,
        Image* images,
        size_t nImages) noexcept
    {
        assert(pMemory);
        assert(pixelSize > 0);
        assert(nImages > 0);

        if (!images)
            return false;

        size_t         index    = 0;
        uint8_t*       pixels   = pMemory;
        const uint8_t* pEndBits = pMemory + pixelSize;

        switch (metadata.dimension)
        {
            case TEX_DIMENSION_TEXTURE1D:
            case TEX_DIMENSION_TEXTURE2D:
            {
                if (metadata.arraySize == 0 || metadata.mipLevels == 0)
                {
                    return false;
                }

                for (size_t item = 0; item < metadata.arraySize; ++item)
                {
                    size_t w = metadata.width;
                    size_t h = metadata.height;

                    for (size_t level = 0; level < metadata.mipLevels; ++level)
                    {
                        if (index >= nImages)
                        {
                            return false;
                        }

                        size_t rowPitch   = 0;
                        size_t slicePitch = 0;

                        if (ComputePitch(metadata.format, w, h, rowPitch, slicePitch, cpFlags) == false)
                            return false;

                        images[index].width = w;
                        images[index].height = h;
                        images[index].format = metadata.format;
                        images[index].rowPitch = rowPitch;
                        images[index].slicePitch = slicePitch;
                        images[index].pixels = pixels;
                        ++index;

                        pixels += slicePitch;
                        if (pixels > pEndBits)
                        {
                            return false;
                        }

                        if (h > 1)
                            h >>= 1;

                        if (w > 1)
                            w >>= 1;
                    }
                }
                return true;
            }

            case TEX_DIMENSION_TEXTURE3D:
            {
                if (metadata.mipLevels == 0 || metadata.depth == 0)
                {
                    return false;
                }

                size_t w = metadata.width;
                size_t h = metadata.height;
                size_t d = metadata.depth;

                for (size_t level = 0; level < metadata.mipLevels; ++level)
                {
                    size_t rowPitch   = 0;
                    size_t slicePitch = 0;

                    if (ComputePitch(metadata.format, w, h, rowPitch, slicePitch, cpFlags) == false)
                        return false;

                    for (size_t slice = 0; slice < d; ++slice)
                    {
                        if (index >= nImages)
                        {
                            return false;
                        }

                        // We use the same memory organization that Direct3D 11 needs for D3D11_SUBRESOURCE_DATA
                        // with all slices of a given miplevel being continuous in memory
                        images[index].width = w;
                        images[index].height = h;
                        images[index].format = metadata.format;
                        images[index].rowPitch = rowPitch;
                        images[index].slicePitch = slicePitch;
                        images[index].pixels = pixels;
                        ++index;

                        pixels += slicePitch;
                        if (pixels > pEndBits)
                        {
                            return false;
                        }
                    }

                    if (h > 1)
                        h >>= 1;

                    if (w > 1)
                        w >>= 1;

                    if (d > 1)
                        d >>= 1;
                }

                return true;
            }

            default:
                return false;
        }
    }

    //-------------------------------------------------------------------------------------
    // Computes the image row pitch in bytes, and the slice ptich (size in bytes of the image)
    // based on VkFormat, width, and height
    //-------------------------------------------------------------------------------------
    bool ComputePitch(
        VkFormat fmt, size_t width, size_t height,
        size_t& rowPitch, size_t& slicePitch, CP_FLAGS flags) noexcept
    {
        uint64_t pitch = 0;
        uint64_t slice = 0;

        switch (static_cast<int>(fmt))
        {
            case VK_FORMAT_UNDEFINED:
                return false;

            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            case VK_FORMAT_BC4_UNORM_BLOCK:
            case VK_FORMAT_BC4_SNORM_BLOCK:
            {
                assert(IsCompressed(fmt));

                if (flags & CP_FLAGS_BAD_DXTN_TAILS)
                {
                    const size_t nbw = width >> 2;
                    const size_t nbh = height >> 2;
                    pitch = std::max<uint64_t>(1u, uint64_t(nbw) * 8u);
                    slice = std::max<uint64_t>(1u, pitch * uint64_t(nbh));
                }
                else
                {
                    const uint64_t nbw = std::max<uint64_t>(1u, (uint64_t(width) + 3u) / 4u);
                    const uint64_t nbh = std::max<uint64_t>(1u, (uint64_t(height) + 3u) / 4u);
                    pitch = nbw * 8u;
                    slice = pitch * nbh;
                }
                break;
            }

            case VK_FORMAT_BC2_UNORM_BLOCK:
            case VK_FORMAT_BC2_SRGB_BLOCK:
            case VK_FORMAT_BC3_UNORM_BLOCK:
            case VK_FORMAT_BC3_SRGB_BLOCK:
            case VK_FORMAT_BC5_UNORM_BLOCK:
            case VK_FORMAT_BC5_SNORM_BLOCK:
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:
            case VK_FORMAT_BC6H_SFLOAT_BLOCK:
            case VK_FORMAT_BC7_UNORM_BLOCK:
            case VK_FORMAT_BC7_SRGB_BLOCK:
            {
                assert(IsCompressed(fmt));

                if (flags & CP_FLAGS_BAD_DXTN_TAILS)
                {
                    const size_t nbw = width >> 2;
                    const size_t nbh = height >> 2;
                    pitch = std::max<uint64_t>(1u, uint64_t(nbw) * 16u);
                    slice = std::max<uint64_t>(1u, pitch * uint64_t(nbh));
                }
                else
                {
                    const uint64_t nbw = std::max<uint64_t>(1u, (uint64_t(width) + 3u) / 4u);
                    const uint64_t nbh = std::max<uint64_t>(1u, (uint64_t(height) + 3u) / 4u);
                    pitch = nbw * 16u;
                    slice = pitch * nbh;
                }

                break;
            }

            case VK_FORMAT_B8G8R8G8_422_UNORM:
            case VK_FORMAT_G8B8G8R8_422_UNORM:
            {
                assert(IsPacked(fmt));

                pitch = ((uint64_t(width) + 1u) >> 1) * 4u;
                slice = pitch * uint64_t(height);
                break;
            }

            case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
            case VK_FORMAT_G16B16G16R16_422_UNORM:
            {
                assert(IsPacked(fmt));

                pitch = ((uint64_t(width) + 1u) >> 1) * 8u;
                slice = pitch * uint64_t(height);
                break;
            }

            case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
            {
                if ((height % 2) != 0)
                {
                    // Requires a height alignment of 2.
                    return false;
                }
                pitch = ((uint64_t(width) + 1u) >> 1) * 2u;
                slice = pitch * (uint64_t(height) + ((uint64_t(height) + 1u) >> 1));
                break;
            }

            case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
            case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
            {
                if ((height % 2) != 0)
                {
                    // Requires a height alignment of 2.
                    return false;
                }

            #if (__cplusplus >= 201703L)
                [[fallthrough]];
            #elif defined(__clang__)
                [[clang::fallthrough]];
            #elif defined(_MSC_VER)
                __fallthrough;
            #endif
            }

            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_R16_UNORM:
            {
                pitch = ((uint64_t(width) + 1u) >> 1) * 4u;
                slice = pitch * (uint64_t(height) + ((uint64_t(height) + 1u) >> 1));
                break;
            }

            default:
            {
                assert(!IsCompressed(fmt) && !IsPacked(fmt) && !IsPlanar(fmt));

                size_t bpp = 0;

                if (flags & CP_FLAGS_24BPP)
                    bpp = 24;
                else if (flags & CP_FLAGS_16BPP)
                    bpp = 16;
                else if (flags & CP_FLAGS_8BPP)
                    bpp = 8;
                else
                    bpp = BitsPerPixel(fmt);

                if (!bpp)
                    return false;

                if (flags & (CP_FLAGS_LEGACY_DWORD | CP_FLAGS_PARAGRAPH | CP_FLAGS_YMM | CP_FLAGS_ZMM | CP_FLAGS_PAGE4K))
                {
                    if (flags & CP_FLAGS_PAGE4K)
                    {
                        pitch = ((uint64_t(width) * bpp + 32767u) / 32768u) * 4096u;
                        slice = pitch * uint64_t(height);
                    }
                    else if (flags & CP_FLAGS_ZMM)
                    {
                        pitch = ((uint64_t(width) * bpp + 511u) / 512u) * 64u;
                        slice = pitch * uint64_t(height);
                    }
                    else if (flags & CP_FLAGS_YMM)
                    {
                        pitch = ((uint64_t(width) * bpp + 255u) / 256u) * 32u;
                        slice = pitch * uint64_t(height);
                    }
                    else if (flags & CP_FLAGS_PARAGRAPH)
                    {
                        pitch = ((uint64_t(width) * bpp + 127u) / 128u) * 16u;
                        slice = pitch * uint64_t(height);
                    }
                    else // DWORD alignment
                    {
                        // Special computation for some incorrectly created DDS files based on
                        // legacy DirectDraw assumptions about pitch alignment
                        pitch = ((uint64_t(width) * bpp + 31u) / 32u) * sizeof(uint32_t);
                        slice = pitch * uint64_t(height);
                    }
                }
                else
                {
                    // Default byte alignment
                    pitch = (uint64_t(width) * bpp + 7u) / 8u;
                    slice = pitch * uint64_t(height);
                }

                break;
            }
        }

    #if defined(_M_IX86) || defined(_M_ARM) || defined(_M_HYBRID_X86_ARM64)
        static_assert(sizeof(size_t) == 4, "Not a 32-bit platform!");
        if (pitch > UINT32_MAX || slice > UINT32_MAX)
        {
            rowPitch = slicePitch = 0;
            return bool_E_ARITHMETIC_OVERFLOW;
        }
    #else
        static_assert(sizeof(size_t) == 8, "Not a 64-bit platform!");
    #endif

        rowPitch = static_cast<size_t>(pitch);
        slicePitch = static_cast<size_t>(slice);

        return true;
    }

    size_t ComputeScanlines(VkFormat fmt, size_t height) noexcept
    {
        switch (static_cast<int>(fmt))
        {
            case VK_FORMAT_UNDEFINED:
                return 0;

            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
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
                return std::max<size_t>(1, (height + 3) / 4);

            case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
            case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
            case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
            case VK_FORMAT_D16_UNORM_S8_UINT:
                return height + ((height + 1) >> 1);

            default:
                assert(IsValid(fmt));
                assert(!IsCompressed(fmt) && !IsPlanar(fmt));
                return height;
        }
    }

    VkFormat MakeSRGB(VkFormat fmt) noexcept
    {
        switch (fmt)
        {
            case VK_FORMAT_R8G8B8A8_UNORM:
                return VK_FORMAT_R8G8B8A8_SRGB;

            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
                return VK_FORMAT_BC1_RGB_SRGB_BLOCK;

            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
                return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;

            case VK_FORMAT_BC2_UNORM_BLOCK:
                return VK_FORMAT_BC2_SRGB_BLOCK;

            case VK_FORMAT_BC3_UNORM_BLOCK:
                return VK_FORMAT_BC3_SRGB_BLOCK;

            case VK_FORMAT_B8G8R8A8_UNORM:
                return VK_FORMAT_B8G8R8A8_SRGB;

            case VK_FORMAT_B8G8R8_UNORM:
                return VK_FORMAT_B8G8R8_SRGB;

            case VK_FORMAT_BC7_UNORM_BLOCK:
                return VK_FORMAT_BC7_SRGB_BLOCK;

            default:
                return fmt;
        }
    }

    //=====================================================================================
    // ScratchImage - Bitmap image container
    //=====================================================================================
    ScratchImage& ScratchImage::operator= (ScratchImage&& moveFrom) noexcept
    {
        if (this != &moveFrom)
        {
            Release();

            m_nimages = moveFrom.m_nimages;
            m_size = moveFrom.m_size;
            m_metadata = moveFrom.m_metadata;
            m_image = moveFrom.m_image;
            m_memory = moveFrom.m_memory;

            moveFrom.m_nimages = 0;
            moveFrom.m_size = 0;
            moveFrom.m_image = nullptr;
            moveFrom.m_memory = nullptr;
        }
        return *this;
    }

    //-------------------------------------------------------------------------------------
    // Methods
    //-------------------------------------------------------------------------------------
    bool ScratchImage::Initialize(const TexMetadata& mdata, CP_FLAGS flags) noexcept
    {
        if (!IsValid(mdata.format))
            return false;

        if (IsPalettized(mdata.format))
            return false;

        size_t mipLevels = mdata.mipLevels;

        switch (mdata.dimension)
        {
            case TEX_DIMENSION_TEXTURE1D:
            {
                if (!mdata.width || mdata.height != 1 || mdata.depth != 1 || !mdata.arraySize)
                    return false;

                if (!CalculateMipLevels(mdata.width, 1, mipLevels))
                    return false;

                break;
            }

            case TEX_DIMENSION_TEXTURE2D:
            {
                if (!mdata.width || !mdata.height || mdata.depth != 1 || !mdata.arraySize)
                    return false;

                if (mdata.IsCubemap())
                {
                    if ((mdata.arraySize % 6) != 0)
                        return false;
                }

                if (!CalculateMipLevels(mdata.width, mdata.height, mipLevels))
                    return false;

                break;
            }

            case TEX_DIMENSION_TEXTURE3D:
            {
                if (!mdata.width || !mdata.height || !mdata.depth || mdata.arraySize != 1)
                    return false;

                if (!CalculateMipLevels3D(mdata.width, mdata.height, mdata.depth, mipLevels))
                    return false;

                break;
            }

            default:
                return false;
        }

        Release();

        m_metadata.width = mdata.width;
        m_metadata.height = mdata.height;
        m_metadata.depth = mdata.depth;
        m_metadata.arraySize = mdata.arraySize;
        m_metadata.mipLevels = mipLevels;
        m_metadata.miscFlags = mdata.miscFlags;
        m_metadata.miscFlags2 = mdata.miscFlags2;
        m_metadata.format = mdata.format;
        m_metadata.dimension = mdata.dimension;

        size_t           pixelSize = 0;
        size_t           nimages   = 0;
        constexpr size_t alignment = 16;

        bool hr = DetermineImageArray(m_metadata, flags, nimages, pixelSize);

        if (hr == false)
            return hr;

        m_image = new (std::nothrow) Image[nimages];

        if (!m_image)
            return false;

        m_nimages = nimages;
        memset(m_image, 0, sizeof(Image) * nimages);

#if _WIN32
        m_memory = static_cast<uint8_t*>(_aligned_malloc(pixelSize, alignment));
#else
        size_t remainder = pixelSize % alignment;

        if (remainder != 0) 
        {
            pixelSize += (alignment - remainder);
        }

        m_memory = static_cast<uint8_t*>(std::aligned_alloc(alignment, pixelSize));
#endif

        if (!m_memory)
        {
            Release();
            return false;
        }

        memset(m_memory, 0, pixelSize);
        m_size = pixelSize;

        if (!SetupImageArray(m_memory, pixelSize, m_metadata, flags, m_image, nimages))
        {
            Release();
            return false;
        }

        return true;
    }

    
    bool ScratchImage::Initialize1D(VkFormat fmt, size_t length, size_t arraySize, size_t mipLevels, CP_FLAGS flags) noexcept
    {
        if (!length || !arraySize)
            return false;

        // 1D is a special case of the 2D case
        bool hr = Initialize2D(fmt, length, 1, arraySize, mipLevels, flags);

        if (hr == false)
            return hr;

        m_metadata.dimension = TEX_DIMENSION_TEXTURE1D;

        return true;
    }

    
    bool ScratchImage::Initialize2D(VkFormat fmt, size_t width, size_t height, size_t arraySize, size_t mipLevels, CP_FLAGS flags) noexcept
    {
        if (!IsValid(fmt) || !width || !height || !arraySize)
            return false;

        if (IsPalettized(fmt))
            return false;

        if (!CalculateMipLevels(width, height, mipLevels))
            return false;

        Release();

        m_metadata.width = width;
        m_metadata.height = height;
        m_metadata.depth = 1;
        m_metadata.arraySize = arraySize;
        m_metadata.mipLevels = mipLevels;
        m_metadata.miscFlags = 0;
        m_metadata.miscFlags2 = 0;
        m_metadata.format = fmt;
        m_metadata.dimension = TEX_DIMENSION_TEXTURE2D;

        size_t           pixelSize = 0; 
        size_t           nimages   = 0;
        constexpr size_t alignment = 16;

        bool hr = DetermineImageArray(m_metadata, flags, nimages, pixelSize);

        if (hr == false)
            return hr;

        m_image = new (std::nothrow) Image[nimages];
        if (!m_image)
            return false;

        m_nimages = nimages;
        memset(m_image, 0, sizeof(Image) * nimages);

#if _WIN32
        m_memory = static_cast<uint8_t*>(_aligned_malloc(pixelSize, alignment));
#else
        size_t remainder = pixelSize % alignment;

        if (remainder != 0) 
        {
            pixelSize += (alignment - remainder);
        }

        m_memory = static_cast<uint8_t*>(std::aligned_alloc(alignment, pixelSize));
#endif

        if (!m_memory)
        {
            Release();
            return false;
        }

        memset(m_memory, 0, pixelSize);
        m_size = pixelSize;

        if (!SetupImageArray(m_memory, pixelSize, m_metadata, flags, m_image, nimages))
        {
            Release();
            return false;
        }

        return true;
    }

    
    bool ScratchImage::Initialize3D(VkFormat fmt, size_t width, size_t height, size_t depth, size_t mipLevels, CP_FLAGS flags) noexcept
    {
        if (!IsValid(fmt) || !width || !height || !depth)
            return false;

        if (depth > INT16_MAX)
            return false;

        if (IsPalettized(fmt))
            return false;

        if (!CalculateMipLevels3D(width, height, depth, mipLevels))
            return false;

        Release();

        m_metadata.width = width;
        m_metadata.height = height;
        m_metadata.depth = depth;
        m_metadata.arraySize = 1;    // Direct3D 10.x/11 does not support arrays of 3D textures
        m_metadata.mipLevels = mipLevels;
        m_metadata.miscFlags = 0;
        m_metadata.miscFlags2 = 0;
        m_metadata.format = fmt;
        m_metadata.dimension = TEX_DIMENSION_TEXTURE3D;

        size_t           pixelSize = 0;
        size_t           nimages   = 0;
        constexpr size_t alignment = 16;

        bool hr = DetermineImageArray(m_metadata, flags, nimages, pixelSize);

        if (hr == false)
            return hr;

        m_image = new (std::nothrow) Image[nimages];

        if (!m_image)
        {
            Release();
            return false;
        }
        m_nimages = nimages;
        memset(m_image, 0, sizeof(Image) * nimages);

#if _WIN32
        m_memory = static_cast<uint8_t*>(_aligned_malloc(pixelSize, alignment));
#else
        size_t remainder = pixelSize % alignment;

        if (remainder != 0) 
        {
            pixelSize += (alignment - remainder);
        }

        m_memory = static_cast<uint8_t*>(std::aligned_alloc(alignment, pixelSize));
#endif

        if (!m_memory)
        {
            Release();
            return false;
        }
        memset(m_memory, 0, pixelSize);
        m_size = pixelSize;

        if (!SetupImageArray(m_memory, pixelSize, m_metadata, flags, m_image, nimages))
        {
            Release();
            return false;
        }

        return true;
    }

    
    bool ScratchImage::InitializeCube(VkFormat fmt, size_t width, size_t height, size_t nCubes, size_t mipLevels, CP_FLAGS flags) noexcept
    {
        if (!width || !height || !nCubes)
            return false;

        // A DirectX11 cubemap is just a 2D texture array that is a multiple of 6 for each cube
        bool hr = Initialize2D(fmt, width, height, nCubes * 6, mipLevels, flags);
        if (hr == false)
            return hr;

        m_metadata.miscFlags |= TEX_MISC_TEXTURECUBE;

        return true;
    }

    
    bool ScratchImage::InitializeFromImage(const Image& srcImage, bool allow1D, CP_FLAGS flags) noexcept
    {
        bool hr = (srcImage.height > 1 || !allow1D)
            ? Initialize2D(srcImage.format, srcImage.width, srcImage.height, 1, 1, flags)
            : Initialize1D(srcImage.format, srcImage.width, 1, 1, flags);

        if (hr == false)
            return hr;

        const size_t rowCount = ComputeScanlines(srcImage.format, srcImage.height);
        if (!rowCount)
            return false;

        const uint8_t* sptr = srcImage.pixels;
        if (!sptr)
            return false;

        uint8_t* dptr = m_image[0].pixels;
        if (!dptr)
            return false;

        const size_t spitch = srcImage.rowPitch;
        const size_t dpitch = m_image[0].rowPitch;

        const size_t size = std::min<size_t>(dpitch, spitch);

        for (size_t y = 0; y < rowCount; ++y)
        {
            memcpy(dptr, sptr, size);
            sptr += spitch;
            dptr += dpitch;
        }

        return true;
    }

    
    bool ScratchImage::InitializeArrayFromImages(const Image* images, size_t nImages, bool allow1D, CP_FLAGS flags) noexcept
    {
        if (!images || !nImages)
            return false;

        const VkFormat format = images[0].format;
        const size_t width = images[0].width;
        const size_t height = images[0].height;

        for (size_t index = 0; index < nImages; ++index)
        {
            if (!images[index].pixels)
                return false;

            if (images[index].format != format || images[index].width != width || images[index].height != height)
            {
                // All images must be the same format, width, and height
                return false;
            }
        }

        bool hr = (height > 1 || !allow1D)
            ? Initialize2D(format, width, height, nImages, 1, flags)
            : Initialize1D(format, width, nImages, 1, flags);

        if (hr == false)
            return hr;

        const size_t rowCount = ComputeScanlines(format, height);
        if (!rowCount)
            return false;

        for (size_t index = 0; index < nImages; ++index)
        {
            const uint8_t* sptr = images[index].pixels;
            if (!sptr)
                return false;

            assert(index < m_nimages);
            uint8_t* dptr = m_image[index].pixels;
            if (!dptr)
                return false;

            const size_t spitch = images[index].rowPitch;
            const size_t dpitch = m_image[index].rowPitch;

            const size_t size = std::min<size_t>(dpitch, spitch);

            for (size_t y = 0; y < rowCount; ++y)
            {
                memcpy(dptr, sptr, size);
                sptr += spitch;
                dptr += dpitch;
            }
        }

        return true;
    }

    
    bool ScratchImage::InitializeCubeFromImages(const Image* images, size_t nImages, CP_FLAGS flags) noexcept
    {
        if (!images || !nImages)
            return false;

        // A DirectX11 cubemap is just a 2D texture array that is a multiple of 6 for each cube
        if ((nImages % 6) != 0)
            return false;

        bool hr = InitializeArrayFromImages(images, nImages, false, flags);
        if (hr == false)
            return hr;

        m_metadata.miscFlags |= TEX_MISC_TEXTURECUBE;

        return true;
    }

    
    bool ScratchImage::Initialize3DFromImages(const Image* images, size_t depth, CP_FLAGS flags) noexcept
    {
        if (!images || !depth)
            return false;

        if (depth > INT16_MAX)
            return false;

        const VkFormat format = images[0].format;
        const size_t width = images[0].width;
        const size_t height = images[0].height;

        for (size_t slice = 0; slice < depth; ++slice)
        {
            if (!images[slice].pixels)
                return false;

            if (images[slice].format != format || images[slice].width != width || images[slice].height != height)
            {
                // All images must be the same format, width, and height
                return false;
            }
        }

        bool hr = Initialize3D(format, width, height, depth, 1, flags);
        if (hr == false)
            return hr;

        const size_t rowCount = ComputeScanlines(format, height);
        if (!rowCount)
            return false;

        for (size_t slice = 0; slice < depth; ++slice)
        {
            const uint8_t* sptr = images[slice].pixels;
            if (!sptr)
                return false;

            assert(slice < m_nimages);
            uint8_t* dptr = m_image[slice].pixels;
            if (!dptr)
                return false;

            const size_t spitch = images[slice].rowPitch;
            const size_t dpitch = m_image[slice].rowPitch;

            const size_t size = std::min<size_t>(dpitch, spitch);

            for (size_t y = 0; y < rowCount; ++y)
            {
                memcpy(dptr, sptr, size);
                sptr += spitch;
                dptr += dpitch;
            }
        }

        return true;
    }

    void ScratchImage::Release() noexcept
    {
        m_nimages = 0;
        m_size = 0;

        if (m_image)
        {
            delete[] m_image;
            m_image = nullptr;
        }

        if (m_memory)
        {
#if _WIN32
            _aligned_free(m_memory);
#else
            std::free(m_memory);
#endif
            m_memory = nullptr;
        }

        memset(&m_metadata, 0, sizeof(m_metadata));
    }

    
    bool ScratchImage::OverrideFormat(VkFormat f) noexcept
    {
        if (!m_image)
            return false;

        if (!IsValid(f) || IsPlanar(f) || IsPalettized(f))
            return false;

        for (size_t index = 0; index < m_nimages; ++index)
        {
            m_image[index].format = f;
        }

        m_metadata.format = f;

        return true;
    }

    
    const Image* ScratchImage::GetImage(size_t mip, size_t item, size_t slice) const noexcept
    {
        if (mip >= m_metadata.mipLevels)
            return nullptr;

        size_t index = 0;

        switch (m_metadata.dimension)
        {
            case TEX_DIMENSION_TEXTURE1D:
            case TEX_DIMENSION_TEXTURE2D:
            {
                if (slice > 0)
                    return nullptr;

                if (item >= m_metadata.arraySize)
                    return nullptr;

                index = item * (m_metadata.mipLevels) + mip;

                break;
            }

            case TEX_DIMENSION_TEXTURE3D:
            {
                if (item > 0)
                {
                    // No support for arrays of volumes
                    return nullptr;
                }
                else
                {
                    size_t d = m_metadata.depth;

                    for (size_t level = 0; level < mip; ++level)
                    {
                        index += d;
                        if (d > 1)
                            d >>= 1;
                    }

                    if (slice >= d)
                        return nullptr;

                    index += slice;
                }

                break;
            }

            default:
                return nullptr;
        }

        return &m_image[index];
    }

    //=====================================================================================
    // Blob - Bitmap image container
    //=====================================================================================
    Blob& Blob::operator=(Blob&& moveFrom) noexcept
    {
        if (this != &moveFrom)
        {
            Release();

            m_buffer = moveFrom.m_buffer;
            m_size = moveFrom.m_size;

            moveFrom.m_buffer = nullptr;
            moveFrom.m_size = 0;
        }

        return *this;
    }

    void Blob::Release() noexcept
    {
        if (m_buffer)
        {
#if _WIN32
            _aligned_free(m_buffer);
#else
            std::free(m_buffer);
#endif
            m_buffer = nullptr;
        }

        m_size = 0;
    }

    bool Blob::Initialize(size_t size) noexcept
    {
        if (!size)
            return false;

        Release();

        constexpr size_t alignment = 16;
#if _WIN32
        m_buffer = reinterpret_cast<uint8_t*>(_aligned_malloc(size, alignment));
#else
        std::size_t remainder = size % alignment;

        if (remainder != 0)
        {
            size += (alignment - remainder);
        }

        m_buffer = reinterpret_cast<uint8_t*>(std::aligned_alloc(alignment, size));
#endif

        if (!m_buffer)
        {
            Release();
            return false;
        }

        m_size = size;

        return true;
    }

    bool Blob::Trim(size_t size) noexcept
    {
        if (!size)
            return false;

        if (!m_buffer)
            return false;

        if (size > m_size)
            return false;

        m_size = size;

        return true;
    }

    bool Blob::Resize(size_t size) noexcept
    {
        if (!size)
            return false;

        if (!m_buffer || !m_size)
            return false;

        constexpr size_t alignment = 16;
#if _WIN32
        auto tbuffer = reinterpret_cast<uint8_t*>(_aligned_malloc(size, alignment));
#else
        std::size_t remainder = size % alignment;

        if (remainder != 0)
        {
            size += (alignment - remainder);
        }

        auto tbuffer = reinterpret_cast<uint8_t*>(std::aligned_alloc(alignment, size));
#endif
        if (!tbuffer)
            return false;

        memcpy(tbuffer, m_buffer, std::min(m_size, size));

        Release();

        m_buffer = tbuffer;
        m_size = size;

        return true;
    }

    //=====================================================================================
    // TexMetadata
    //=====================================================================================
    size_t TexMetadata::ComputeIndex(size_t mip, size_t item, size_t slice) const noexcept
    {
        if (mip >= mipLevels)
            return size_t(-1);

        switch (dimension)
        {
            case TEX_DIMENSION_TEXTURE1D:
            case TEX_DIMENSION_TEXTURE2D:
            {
                if (slice > 0)
                    return size_t(-1);

                if (item >= arraySize)
                    return size_t(-1);

                return (item * mipLevels + mip);
            }

            case TEX_DIMENSION_TEXTURE3D:
            {
                if (item > 0)
                {
                    // No support for arrays of volumes
                    return static_cast<size_t>(-1);
                }
                else
                {
                    size_t index = 0;
                    size_t d = depth;

                    for (size_t level = 0; level < mip; ++level)
                    {
                        index += d;
                        if (d > 1)
                            d >>= 1;
                    }

                    if (slice >= d)
                        return static_cast<size_t>(-1);;

                    index += slice;

                    return index;
                }
            }

            default:
                return static_cast<size_t>(-1);;
        }
    }

    // Equivalent to D3D11CacluateSubresource: MipSlice + ArraySlice * MipLevels
    uint32_t TexMetadata::CalculateSubresource(size_t mip, size_t item) const noexcept
    {
        uint32_t result = static_cast<uint32_t>(-1);

        if (mip < mipLevels)
        {
            switch (dimension)
            {
                case TEX_DIMENSION_TEXTURE1D:
                case TEX_DIMENSION_TEXTURE2D:
                {
                    if (item < arraySize)
                    {
                        result = static_cast<uint32_t>(mip + item * mipLevels);
                    }

                    break;
                }

                case TEX_DIMENSION_TEXTURE3D:
                {
                    // No support for arrays of volumes
                    if (item == 0)
                    {
                        result = static_cast<uint32_t>(mip);
                    }

                    break;
                }

                default:
                    break;
            }
        }

        return result;
    }

    // Equivalent to D3D12CacluateSubresource: MipSlice + ArraySlice * MipLevels + PlaneSlice * MipLevels * ArraySize
    uint32_t TexMetadata::CalculateSubresource(size_t mip, size_t item, size_t plane) const noexcept
    {
        uint32_t result = static_cast<uint32_t>(-1);

        if (mip < mipLevels)
        {
            switch (dimension)
            {
                case TEX_DIMENSION_TEXTURE1D:
                case TEX_DIMENSION_TEXTURE2D:
                {
                    if (item < arraySize)
                    {
                        result = static_cast<uint32_t>(mip + item * mipLevels +
                                                       plane * mipLevels * arraySize);
                    }

                    break;
                }

                case TEX_DIMENSION_TEXTURE3D:
                {
                    // No support for arrays of volumes
                    if (item == 0)
                    {
                        result = static_cast<uint32_t>(mip + plane * mipLevels);
                    }

                    break;
                }

                default:
                    break;
            }
        }

        return result;
    }

    bool IsCompressed(VkFormat fmt) noexcept
    {
        switch (fmt)
        {
            // ====================================================
            // BC (Block Compression)
            // ====================================================
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
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
            // ====================================================
            // ETC2 / EAC
            // ====================================================
            case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
            case VK_FORMAT_EAC_R11_UNORM_BLOCK:
            case VK_FORMAT_EAC_R11_SNORM_BLOCK:
            case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
            case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
            // ====================================================
            // ASTC (Adaptive Scalable Texture Compression) - LDR
            // ====================================================
            case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
            case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
            case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
            case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
            case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
            case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
            case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
            case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
            case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
            case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
            case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
            case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
            case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
            case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
            // ====================================================
            // ASTC HDR (High Dynamic Range)
            // ====================================================
            case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT:
            case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT:
            // ====================================================
            // PVRTC (PowerVR)
            // ====================================================
            case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
            case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
                return true;
            default:
                return false;
        }
    }

    bool IsPacked(VkFormat fmt) noexcept
    {
        switch (fmt)
        {
            // ====================================================
            // 8-bit Packed Formats
            // ====================================================
            case VK_FORMAT_R4G4_UNORM_PACK8:
            // ====================================================
            // 16-bit Packed Formats
            // ====================================================
            // R4G4B4A4
            case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
            case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
            // R5G6B5
            case VK_FORMAT_R5G6B5_UNORM_PACK16:
            case VK_FORMAT_B5G6R5_UNORM_PACK16:
            // R5G5B5A1
            case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
            case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
            case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
            // ====================================================
            // 32-bit Packed Formats
            // ====================================================
            // A8B8G8R8
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
            case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
            case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
            case VK_FORMAT_A8B8G8R8_UINT_PACK32:
            case VK_FORMAT_A8B8G8R8_SINT_PACK32:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            // A2R10G10B10
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
            case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
            case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
            case VK_FORMAT_A2R10G10B10_UINT_PACK32:
            case VK_FORMAT_A2R10G10B10_SINT_PACK32:
            // A2B10G10R10
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
            case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
            case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
            case VK_FORMAT_A2B10G10R10_UINT_PACK32:
            case VK_FORMAT_A2B10G10R10_SINT_PACK32:

            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
            case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
            
            case VK_FORMAT_X8_D24_UNORM_PACK32:

            case VK_FORMAT_B8G8R8G8_422_UNORM:
            case VK_FORMAT_G8B8G8R8_422_UNORM:
            case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
            case VK_FORMAT_G16B16G16R16_422_UNORM:
                return true;

            // ====================================================
            // Array formats, Compressed formats, etc.
            // ====================================================
            default:
                return false;
        }
    }

    bool IsVideo(VkFormat fmt) noexcept
    {
        switch (fmt) 
        {
            // ====================================================
            // 8-bit YCbCr Formats (Common)
            // ====================================================
            // Packed 4:2:2 (Single Plane)
            case VK_FORMAT_G8B8G8R8_422_UNORM:
            case VK_FORMAT_B8G8R8G8_422_UNORM:
            // Multi-planar 4:2:0
            case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
            case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
            // Multi-planar 4:2:2
            case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
            case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
            // Multi-planar 4:4:4
            case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:

            // ====================================================
            // 10-bit / 12-bit / 16-bit YCbCr Formats (HDR / Pro Video)
            // ====================================================
            // --- 10-bit ---
            // Packed 4:2:2
            case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
            case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
            // Multi-planar 4:2:0
            case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
            case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
            // Multi-planar 4:2:2
            case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
            case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
            // Multi-planar 4:4:4
            case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
            // --- 12-bit ---
            // Packed 4:2:2
            case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
            case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
            // Multi-planar 4:2:0
            case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
            case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
            // Multi-planar 4:2:2
            case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
            case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
            // Multi-planar 4:4:4
            case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
            // --- 16-bit ---
            // Packed 4:2:2
            case VK_FORMAT_G16B16G16R16_422_UNORM:
            case VK_FORMAT_B16G16R16G16_422_UNORM:
            // Multi-planar 4:2:0
            case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
            case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
            // Multi-planar 4:2:2
            case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
            case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
            // Multi-planar 4:4:4
            case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
                return true;

            default:
                return false;
        }
    }

    bool IsPlanar(VkFormat fmt, bool isd3d12) noexcept
    {
        switch (static_cast<int>(fmt))
        {
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
                return isd3d12; // Direct3D 12 considers these planar, Direct3D 11 does not.

            default:
                return false;
        }
    }

    bool IsPalettized(VkFormat fmt) noexcept
    {
        return false;
    }

    bool IsDepthStencil(VkFormat fmt) noexcept
    {
        switch (static_cast<int>(fmt))
        {
            case VK_FORMAT_D16_UNORM :
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_S8_UINT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return true;

            default:
                return false;
        }
    }

    bool IsSRGB(VkFormat fmt) noexcept
    {
        switch (fmt)
        {
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
        case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
            return true;

        default:
            return false;
        }
    }

    bool IsBGR(VkFormat fmt) noexcept
    {
        switch (static_cast<int>(fmt))
        {
            case VK_FORMAT_B5G6R5_UNORM_PACK16:
            case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
            case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
            case VK_FORMAT_B8G8R8_UNORM:
            case VK_FORMAT_B8G8R8_SNORM:
            case VK_FORMAT_B8G8R8_USCALED:
            case VK_FORMAT_B8G8R8_SSCALED:
            case VK_FORMAT_B8G8R8_UINT:
            case VK_FORMAT_B8G8R8_SINT:
            case VK_FORMAT_B8G8R8_SRGB:
            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_B8G8R8A8_SNORM:
            case VK_FORMAT_B8G8R8A8_USCALED:
            case VK_FORMAT_B8G8R8A8_SSCALED:
            case VK_FORMAT_B8G8R8A8_UINT:
            case VK_FORMAT_B8G8R8A8_SINT:
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
            case VK_FORMAT_B8G8R8G8_422_UNORM:
                return true;

            default:
                return false;
        }
    }

    bool IsTypeless(VkFormat fmt, bool partialTypeless) noexcept
    {
        return false;
    }

    bool HasAlpha(VkFormat fmt) noexcept
    {
        switch (fmt) 
        {
            // 4-bit / 5-bit / 1-bit Alpha
            case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
            case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
            case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
            case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
            case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
            
            // 8-bit Alpha
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SNORM:
            case VK_FORMAT_R8G8B8A8_USCALED:
            case VK_FORMAT_R8G8B8A8_SSCALED:
            case VK_FORMAT_R8G8B8A8_UINT:
            case VK_FORMAT_R8G8B8A8_SINT:
            case VK_FORMAT_R8G8B8A8_SRGB:
                
            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_B8G8R8A8_SNORM:
            case VK_FORMAT_B8G8R8A8_USCALED:
            case VK_FORMAT_B8G8R8A8_SSCALED:
            case VK_FORMAT_B8G8R8A8_UINT:
            case VK_FORMAT_B8G8R8A8_SINT:
            case VK_FORMAT_B8G8R8A8_SRGB:
                
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
            case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
            case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
            case VK_FORMAT_A8B8G8R8_UINT_PACK32:
            case VK_FORMAT_A8B8G8R8_SINT_PACK32:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
                
            // 2-bit Alpha (10-10-10-2)
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
            case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
            case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
            case VK_FORMAT_A2R10G10B10_UINT_PACK32:
            case VK_FORMAT_A2R10G10B10_SINT_PACK32:
                
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
            case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
            case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
            case VK_FORMAT_A2B10G10R10_UINT_PACK32:
            case VK_FORMAT_A2B10G10R10_SINT_PACK32:

            // 16-bit / channel
            case VK_FORMAT_R16G16B16A16_UNORM:
            case VK_FORMAT_R16G16B16A16_SNORM:
            case VK_FORMAT_R16G16B16A16_USCALED:
            case VK_FORMAT_R16G16B16A16_SSCALED:
            case VK_FORMAT_R16G16B16A16_UINT:
            case VK_FORMAT_R16G16B16A16_SINT:
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                
            // 32-bit / channel
            case VK_FORMAT_R32G32B32A32_UINT:
            case VK_FORMAT_R32G32B32A32_SINT:
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                
            // 64-bit / channel
            case VK_FORMAT_R64G64B64A64_UINT:
            case VK_FORMAT_R64G64B64A64_SINT:
            case VK_FORMAT_R64G64B64A64_SFLOAT:

            // BC1 (DXT1)
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
                
            // BC2 (DXT3) - Explicit Alpha
            case VK_FORMAT_BC2_UNORM_BLOCK:
            case VK_FORMAT_BC2_SRGB_BLOCK:
                
            // BC3 (DXT5) - Interpolated Alpha
            case VK_FORMAT_BC3_UNORM_BLOCK:
            case VK_FORMAT_BC3_SRGB_BLOCK:
                
            // BC7 - Modern High Quality
            case VK_FORMAT_BC7_UNORM_BLOCK:
            case VK_FORMAT_BC7_SRGB_BLOCK:

            // ETC2 - Explicit variants with Alpha
            case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
            
            // ASTC
            case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
            case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
            case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
            case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
            case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
            case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
            case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
            case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
            case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
            case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
            case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
            case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
            case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
            case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
            case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK:

            // 5. PVRTC
            case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
            case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:

            // VK_KHR_maintenance5
            case VK_FORMAT_A8_UNORM_KHR: 
                return true;

            default:
                return false;
        }
    }

    size_t BitsPerPixel(VkFormat fmt) noexcept
    {
        switch (fmt)
        {
            // ====================================================
            // 8-bit (1 Byte)
            // ====================================================
            case VK_FORMAT_R8_UNORM:
            case VK_FORMAT_R8_SNORM:
            case VK_FORMAT_R8_USCALED:
            case VK_FORMAT_R8_SSCALED:
            case VK_FORMAT_R8_UINT:
            case VK_FORMAT_R8_SINT:
            case VK_FORMAT_R8_SRGB:
            case VK_FORMAT_S8_UINT:
            case VK_FORMAT_R4G4_UNORM_PACK8:
                return 8;

            // ====================================================
            // 16-bit (2 Bytes)
            // ====================================================
            case VK_FORMAT_R8G8_UNORM:
            case VK_FORMAT_R8G8_SNORM:
            case VK_FORMAT_R8G8_USCALED:
            case VK_FORMAT_R8G8_SSCALED:
            case VK_FORMAT_R8G8_UINT:
            case VK_FORMAT_R8G8_SINT:
            case VK_FORMAT_R8G8_SRGB:
            case VK_FORMAT_R16_UNORM:
            case VK_FORMAT_R16_SNORM:
            case VK_FORMAT_R16_USCALED:
            case VK_FORMAT_R16_SSCALED:
            case VK_FORMAT_R16_UINT:
            case VK_FORMAT_R16_SINT:
            case VK_FORMAT_R16_SFLOAT:
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
            case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
            case VK_FORMAT_R5G6B5_UNORM_PACK16:
            case VK_FORMAT_B5G6R5_UNORM_PACK16:
            case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
            case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
            case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
            case VK_FORMAT_R10X6_UNORM_PACK16:
            case VK_FORMAT_R12X4_UNORM_PACK16:
                return 16;

            // ====================================================
            // 24-bit (3 Bytes)
            // ====================================================
            case VK_FORMAT_R8G8B8_UNORM:
            case VK_FORMAT_R8G8B8_SNORM:
            case VK_FORMAT_R8G8B8_USCALED:
            case VK_FORMAT_R8G8B8_SSCALED:
            case VK_FORMAT_R8G8B8_UINT:
            case VK_FORMAT_R8G8B8_SINT:
            case VK_FORMAT_R8G8B8_SRGB:
            case VK_FORMAT_B8G8R8_UNORM:
            case VK_FORMAT_B8G8R8_SNORM:
            case VK_FORMAT_B8G8R8_USCALED:
            case VK_FORMAT_B8G8R8_SSCALED:
            case VK_FORMAT_B8G8R8_UINT:
            case VK_FORMAT_B8G8R8_SINT:
            case VK_FORMAT_B8G8R8_SRGB:
            case VK_FORMAT_D16_UNORM_S8_UINT:
                return 24;

            // ====================================================
            // 32-bit (4 Bytes)
            // ====================================================
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SNORM:
            case VK_FORMAT_R8G8B8A8_USCALED:
            case VK_FORMAT_R8G8B8A8_SSCALED:
            case VK_FORMAT_R8G8B8A8_UINT:
            case VK_FORMAT_R8G8B8A8_SINT:
            case VK_FORMAT_R8G8B8A8_SRGB:
            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_B8G8R8A8_SNORM:
            case VK_FORMAT_B8G8R8A8_USCALED:
            case VK_FORMAT_B8G8R8A8_SSCALED:
            case VK_FORMAT_B8G8R8A8_UINT:
            case VK_FORMAT_B8G8R8A8_SINT:
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
            case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
            case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
            case VK_FORMAT_A8B8G8R8_UINT_PACK32:
            case VK_FORMAT_A8B8G8R8_SINT_PACK32:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
            case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
            case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
            case VK_FORMAT_A2R10G10B10_UINT_PACK32:
            case VK_FORMAT_A2R10G10B10_SINT_PACK32:
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
            case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
            case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
            case VK_FORMAT_A2B10G10R10_UINT_PACK32:
            case VK_FORMAT_A2B10G10R10_SINT_PACK32:
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
            case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
            case VK_FORMAT_R16G16_UNORM:
            case VK_FORMAT_R16G16_SNORM:
            case VK_FORMAT_R16G16_USCALED:
            case VK_FORMAT_R16G16_SSCALED:
            case VK_FORMAT_R16G16_UINT:
            case VK_FORMAT_R16G16_SINT:
            case VK_FORMAT_R16G16_SFLOAT:
            case VK_FORMAT_R32_UINT:
            case VK_FORMAT_R32_SINT:
            case VK_FORMAT_R32_SFLOAT:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D24_UNORM_S8_UINT: // 24 depth + 8 stencil
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
            case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
                return 32;

            // ====================================================
            // 48-bit (6 Bytes)
            // ====================================================
            case VK_FORMAT_R16G16B16_UNORM:
            case VK_FORMAT_R16G16B16_SNORM:
            case VK_FORMAT_R16G16B16_USCALED:
            case VK_FORMAT_R16G16B16_SSCALED:
            case VK_FORMAT_R16G16B16_UINT:
            case VK_FORMAT_R16G16B16_SINT:
            case VK_FORMAT_R16G16B16_SFLOAT:
                return 48;

            // ====================================================
            // 64-bit (8 Bytes)
            // ====================================================
            case VK_FORMAT_R16G16B16A16_UNORM:
            case VK_FORMAT_R16G16B16A16_SNORM:
            case VK_FORMAT_R16G16B16A16_USCALED:
            case VK_FORMAT_R16G16B16A16_SSCALED:
            case VK_FORMAT_R16G16B16A16_UINT:
            case VK_FORMAT_R16G16B16A16_SINT:
            case VK_FORMAT_R16G16B16A16_SFLOAT:
            case VK_FORMAT_R32G32_UINT:
            case VK_FORMAT_R32G32_SINT:
            case VK_FORMAT_R32G32_SFLOAT:
            case VK_FORMAT_R64_UINT:
            case VK_FORMAT_R64_SINT:
            case VK_FORMAT_R64_SFLOAT:
                return 64;

            // ====================================================
            // 96-bit (12 Bytes)
            // ====================================================
            case VK_FORMAT_R32G32B32_UINT:
            case VK_FORMAT_R32G32B32_SINT:
            case VK_FORMAT_R32G32B32_SFLOAT:
                return 96;

            // ====================================================
            // 128-bit (16 Bytes)
            // ====================================================
            case VK_FORMAT_R32G32B32A32_UINT:
            case VK_FORMAT_R32G32B32A32_SINT:
            case VK_FORMAT_R32G32B32A32_SFLOAT:
            case VK_FORMAT_R64G64_UINT:
            case VK_FORMAT_R64G64_SINT:
            case VK_FORMAT_R64G64_SFLOAT:
                return 128;

            // ====================================================
            // 192-bit (24 Bytes)
            // ====================================================
            case VK_FORMAT_R64G64B64_UINT:
            case VK_FORMAT_R64G64B64_SINT:
            case VK_FORMAT_R64G64B64_SFLOAT:
                return 192;

            // ====================================================
            // 256-bit (32 Bytes)
            // ====================================================
            case VK_FORMAT_R64G64B64A64_UINT:
            case VK_FORMAT_R64G64B64A64_SINT:
            case VK_FORMAT_R64G64B64A64_SFLOAT:
                return 256;

            // BC1: 64 bits per 4x4 block = 4 bpp
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            case VK_FORMAT_BC4_UNORM_BLOCK:
            case VK_FORMAT_BC4_SNORM_BLOCK:
                return 4;

            // BC2/3/5/6/7: 128 bits per 4x4 block = 8 bpp
            case VK_FORMAT_BC2_UNORM_BLOCK:
            case VK_FORMAT_BC2_SRGB_BLOCK:
            case VK_FORMAT_BC3_UNORM_BLOCK:
            case VK_FORMAT_BC3_SRGB_BLOCK:
            case VK_FORMAT_BC5_UNORM_BLOCK:
            case VK_FORMAT_BC5_SNORM_BLOCK:
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:
            case VK_FORMAT_BC6H_SFLOAT_BLOCK:
            case VK_FORMAT_BC7_UNORM_BLOCK:
            case VK_FORMAT_BC7_SRGB_BLOCK:
                return 8;

            // ETC2 RGB / EAC R11: 64 bits per 4x4 block = 4 bpp
            case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
            case VK_FORMAT_EAC_R11_UNORM_BLOCK:
            case VK_FORMAT_EAC_R11_SNORM_BLOCK:
                return 4;

            // ETC2 RGBA / EAC RG11: 128 bits per 4x4 block = 8 bpp
            case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
            case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
            case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
                return 8;

            case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
                return 2;

            case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
                return 4;

            // ASTC 4x4 - 128bits / 16pixels = 8bpp
            // (128 * blocks) / (width * height)
            case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
            case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
            case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK:
                return 8;

            default:
                return 0;
        }
    }

    size_t BitsPerColor(VkFormat fmt) noexcept
    {
        switch (fmt)
        {
            // ====================================================
            // 4-bit Channel
            // ====================================================
            case VK_FORMAT_R4G4_UNORM_PACK8:
            case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
            case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
                return 4;

            // ====================================================
            // 5-bit / 6-bit Mixed (Packed)
            // ====================================================
            case VK_FORMAT_R5G6B5_UNORM_PACK16:
            case VK_FORMAT_B5G6R5_UNORM_PACK16:
                return 6;

            case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
            case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
            case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
                return 5;

            // ====================================================
            // 8-bit Channel
            // ====================================================
            case VK_FORMAT_R8_UNORM:
            case VK_FORMAT_R8_SNORM:
            case VK_FORMAT_R8_USCALED:
            case VK_FORMAT_R8_SSCALED:
            case VK_FORMAT_R8_UINT:
            case VK_FORMAT_R8_SINT:
            case VK_FORMAT_R8_SRGB:
            case VK_FORMAT_R8G8_UNORM:
            case VK_FORMAT_R8G8_SNORM:
            case VK_FORMAT_R8G8_USCALED:
            case VK_FORMAT_R8G8_SSCALED:
            case VK_FORMAT_R8G8_UINT:
            case VK_FORMAT_R8G8_SINT:
            case VK_FORMAT_R8G8_SRGB:
            case VK_FORMAT_R8G8B8_UNORM:
            case VK_FORMAT_R8G8B8_SNORM:
            case VK_FORMAT_R8G8B8_USCALED:
            case VK_FORMAT_R8G8B8_SSCALED:
            case VK_FORMAT_R8G8B8_UINT:
            case VK_FORMAT_R8G8B8_SINT:
            case VK_FORMAT_R8G8B8_SRGB:
            case VK_FORMAT_B8G8R8_UNORM:
            case VK_FORMAT_B8G8R8_SNORM:
            case VK_FORMAT_B8G8R8_USCALED:
            case VK_FORMAT_B8G8R8_SSCALED:
            case VK_FORMAT_B8G8R8_UINT:
            case VK_FORMAT_B8G8R8_SINT:
            case VK_FORMAT_B8G8R8_SRGB:
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SNORM:
            case VK_FORMAT_R8G8B8A8_USCALED:
            case VK_FORMAT_R8G8B8A8_SSCALED:
            case VK_FORMAT_R8G8B8A8_UINT:
            case VK_FORMAT_R8G8B8A8_SINT:
            case VK_FORMAT_R8G8B8A8_SRGB:
            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_B8G8R8A8_SNORM:
            case VK_FORMAT_B8G8R8A8_USCALED:
            case VK_FORMAT_B8G8R8A8_SSCALED:
            case VK_FORMAT_B8G8R8A8_UINT:
            case VK_FORMAT_B8G8R8A8_SINT:
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
            case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
            case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
            case VK_FORMAT_A8B8G8R8_UINT_PACK32:
            case VK_FORMAT_A8B8G8R8_SINT_PACK32:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            case VK_FORMAT_S8_UINT: 
            case VK_FORMAT_A8_UNORM_KHR:
                return 8;

            // ====================================================
            // 10-bit Channel (RGB10A2)
            // ====================================================
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
            case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
            case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
            case VK_FORMAT_A2R10G10B10_UINT_PACK32:
            case VK_FORMAT_A2R10G10B10_SINT_PACK32:
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
            case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
            case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
            case VK_FORMAT_A2B10G10R10_UINT_PACK32:
            case VK_FORMAT_A2B10G10R10_SINT_PACK32:
            case VK_FORMAT_R10X6_UNORM_PACK16:         // 10 bits effective
            case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:   // 10 bits effective
                return 10;

            // ====================================================
            // 11-bit Channel (Special Float)
            // B10G11R11
            // ====================================================
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
                return 11;
                
            // ====================================================
            // 12-bit Channel
            // ====================================================
            case VK_FORMAT_R12X4_UNORM_PACK16:
            case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
                return 12;

            // ====================================================
            // 16-bit Channel (Half Float / Short)
            // ====================================================
            case VK_FORMAT_R16_UNORM:
            case VK_FORMAT_R16_SNORM:
            case VK_FORMAT_R16_USCALED:
            case VK_FORMAT_R16_SSCALED:
            case VK_FORMAT_R16_UINT:
            case VK_FORMAT_R16_SINT:
            case VK_FORMAT_R16_SFLOAT:
            case VK_FORMAT_R16G16_UNORM:
            case VK_FORMAT_R16G16_SNORM:
            case VK_FORMAT_R16G16_USCALED:
            case VK_FORMAT_R16G16_SSCALED:
            case VK_FORMAT_R16G16_UINT:
            case VK_FORMAT_R16G16_SINT:
            case VK_FORMAT_R16G16_SFLOAT:
            case VK_FORMAT_R16G16B16_UNORM:
            case VK_FORMAT_R16G16B16_SNORM:
            case VK_FORMAT_R16G16B16_USCALED:
            case VK_FORMAT_R16G16B16_SSCALED:
            case VK_FORMAT_R16G16B16_UINT:
            case VK_FORMAT_R16G16B16_SINT:
            case VK_FORMAT_R16G16B16_SFLOAT:
            case VK_FORMAT_R16G16B16A16_UNORM:
            case VK_FORMAT_R16G16B16A16_SNORM:
            case VK_FORMAT_R16G16B16A16_USCALED:
            case VK_FORMAT_R16G16B16A16_SSCALED:
            case VK_FORMAT_R16G16B16A16_UINT:
            case VK_FORMAT_R16G16B16A16_SINT:
            case VK_FORMAT_R16G16B16A16_SFLOAT:
            // 16-bit Depth
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D16_UNORM_S8_UINT: // Depth is 16
                return 16;

            // ====================================================
            // 24-bit Channel (Depth Only)
            // ====================================================
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
                return 24;

            // ====================================================
            // 32-bit Channel (Float / Int)
            // ====================================================
            case VK_FORMAT_R32_UINT:
            case VK_FORMAT_R32_SINT:
            case VK_FORMAT_R32_SFLOAT:
            case VK_FORMAT_R32G32_UINT:
            case VK_FORMAT_R32G32_SINT:
            case VK_FORMAT_R32G32_SFLOAT:
            case VK_FORMAT_R32G32B32_UINT:
            case VK_FORMAT_R32G32B32_SINT:
            case VK_FORMAT_R32G32B32_SFLOAT:
            case VK_FORMAT_R32G32B32A32_UINT:
            case VK_FORMAT_R32G32B32A32_SINT:
            case VK_FORMAT_R32G32B32A32_SFLOAT:
            // 32-bit Depth
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return 32;

            // ====================================================
            // 64-bit Channel (Double)
            // ====================================================
            case VK_FORMAT_R64_UINT:
            case VK_FORMAT_R64_SINT:
            case VK_FORMAT_R64_SFLOAT:
            case VK_FORMAT_R64G64_UINT:
            case VK_FORMAT_R64G64_SINT:
            case VK_FORMAT_R64G64_SFLOAT:
            case VK_FORMAT_R64G64B64_UINT:
            case VK_FORMAT_R64G64B64_SINT:
            case VK_FORMAT_R64G64B64_SFLOAT:
            case VK_FORMAT_R64G64B64A64_UINT:
            case VK_FORMAT_R64G64B64A64_SINT:
            case VK_FORMAT_R64G64B64A64_SFLOAT:
                return 64;

            // ====================================================
            // Compressed / Shared Exp)
            // ====================================================
            case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: // Shared exponent, mantissa is 9
                return 9; 

            default:
                // Compressed formats (BC, ETC, ASTC) don't have a single "bits per color".
                // Planar formats (YUV) usually handled separately.
                return 0;
        }
    }

    size_t BytesPerBlock(VkFormat fmt) noexcept
    {
        switch (fmt)
        {
            // ====================================================
            // 8 Bytes (64 bits) per Block
            // ====================================================
            
            // BC1 (DXT1)
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            
            // BC4 (1 Channel)
            case VK_FORMAT_BC4_UNORM_BLOCK:
            case VK_FORMAT_BC4_SNORM_BLOCK:
            
            // ETC2 (RGB & RGB+1bit Alpha)
            case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
            
            // EAC (1 Channel)
            case VK_FORMAT_EAC_R11_UNORM_BLOCK:
            case VK_FORMAT_EAC_R11_SNORM_BLOCK:

            // PVRTC
            case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
            case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
            case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
            case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
                return 8;

            // ====================================================
            // 16 Bytes (128 bits) per Block
            // ====================================================

            // BC2 (DXT3)
            case VK_FORMAT_BC2_UNORM_BLOCK:
            case VK_FORMAT_BC2_SRGB_BLOCK:
            
            // BC3 (DXT5)
            case VK_FORMAT_BC3_UNORM_BLOCK:
            case VK_FORMAT_BC3_SRGB_BLOCK:
            
            // BC5 (2 Channel)
            case VK_FORMAT_BC5_UNORM_BLOCK:
            case VK_FORMAT_BC5_SNORM_BLOCK:
            
            // BC6H (HDR)
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:
            case VK_FORMAT_BC6H_SFLOAT_BLOCK:
            
            // BC7 (High Quality)
            case VK_FORMAT_BC7_UNORM_BLOCK:
            case VK_FORMAT_BC7_SRGB_BLOCK:

            // ETC2 (RGBA 8-bit Alpha)
            case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
            
            // EAC (2 Channel)
            case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
            case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:

            // ASTC
            case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
            case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
            case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
            case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
            case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
            case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
            case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
            case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
            case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
            case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
            case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
            case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
            case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
            case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
            case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK:
            case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
            case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
            case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK:
                return 16;

            default:
                // Consider special formats
                return 0;
        }
    }

    //=====================================================================================
    // Image I/O
    //=====================================================================================
    bool SaveToDDSMemory(const Image& image, DDS_FLAGS flags, Blob& blob) noexcept
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


    bool SaveToDDSFile(const Image& image, DDS_FLAGS flags, const char* szFile) noexcept
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
    bool GetMetadataFromTGAMemory(const uint8_t* pSource, size_t size, TexMetadata& metadata) noexcept
    {
        return GetMetadataFromTGAMemory(pSource, size, TGA_FLAGS_NONE, metadata);
    }


    bool GetMetadataFromTGAFile(const char* szFile, TexMetadata& metadata) noexcept
    {
        return GetMetadataFromTGAFile(szFile, TGA_FLAGS_NONE, metadata);
    }


    bool LoadFromTGAMemory(const uint8_t* pSource, size_t size, TexMetadata* metadata, ScratchImage& image) noexcept
    {
        return LoadFromTGAMemory(pSource, size, TGA_FLAGS_NONE, metadata, image);
    }


    bool LoadFromTGAFile(const char* szFile, TexMetadata* metadata, ScratchImage& image) noexcept
    {
        return LoadFromTGAFile(szFile, TGA_FLAGS_NONE, metadata, image);
    }


    bool SaveToTGAMemory(const Image& image, Blob& blob, const TexMetadata* metadata) noexcept
    {
        return SaveToTGAMemory(image, TGA_FLAGS_NONE, blob, metadata);
    }


    bool SaveToTGAFile(const Image& image, const char* szFile, const TexMetadata* metadata) noexcept
    {
        return SaveToTGAFile(image, TGA_FLAGS_NONE, szFile, metadata);
    }

    //=====================================================================================
    // C++17 helpers
    //=====================================================================================
#ifdef __cpp_lib_byte
    bool GetMetadataFromDDSMemory(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata& metadata) noexcept
    {
        return GetMetadataFromDDSMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata);
    }


    bool LoadFromDDSMemory(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata* metadata, ScratchImage& image) noexcept
    {
        return LoadFromDDSMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, image);
    }


    bool GetMetadataFromDDSMemoryEx(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata& metadata, DDSMetaData* ddPixelFormat) noexcept
    {
        return GetMetadataFromDDSMemoryEx(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, ddPixelFormat);
    }


    bool LoadFromDDSMemoryEx(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata* metadata, DDSMetaData* ddPixelFormat, ScratchImage& image) noexcept
    {
        return LoadFromDDSMemoryEx(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, ddPixelFormat, image);
    }

    bool GetMetadataFromTGAMemory(const std::byte* pSource, size_t size, TGA_FLAGS flags, TexMetadata& metadata) noexcept
    {
        return GetMetadataFromTGAMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata);
    }


    bool LoadFromTGAMemory(const std::byte* pSource, size_t size, TGA_FLAGS flags, TexMetadata* metadata, ScratchImage& image) noexcept
    {
        return LoadFromTGAMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, image);
    }


    bool EncodeDDSHeader(const TexMetadata& metadata, DDS_FLAGS flags, std::byte* pDestination, size_t maxsize, size_t& required) noexcept
    {
        return EncodeDDSHeader(metadata, flags, reinterpret_cast<uint8_t*>(pDestination), maxsize, required);
    }


    bool EncodeDDSHeader(const TexMetadata& metadata, DDS_FLAGS flags, std::nullptr_t, size_t maxsize, size_t& required) noexcept
    {
        return EncodeDDSHeader(metadata, flags, static_cast<uint8_t*>(nullptr), maxsize, required);
    }
#endif // __cpp_lib_byte

    //-------------------------------------------------------------------------------------
    // Copies an image row with optional clearing of alpha value to 1.0
    // (can be used in place as well) otherwise copies the image row unmodified.
    //-------------------------------------------------------------------------------------
    void CopyScanline(
        void* pDestination,
        size_t outSize,
        const void* pSource,
        size_t inSize,
        VkFormat format,
        uint32_t tflags) noexcept
    {
        assert(pDestination && outSize > 0);
        assert(pSource && inSize > 0);
        assert(IsValid(format) && !IsPalettized(format));

        if (tflags & TEXP_SCANLINE_SETALPHA)
        {
            switch (static_cast<int>(format))
            {
                //-----------------------------------------------------------------------------
                case VK_FORMAT_R32G32B32A32_SFLOAT:
                case VK_FORMAT_R32G32B32A32_UINT:
                case VK_FORMAT_R32G32B32A32_SINT:
                {
                    if (inSize >= 16 && outSize >= 16)
                    {
                        uint32_t alpha;
                        if (format == VK_FORMAT_R32G32B32A32_SFLOAT)
                            alpha = 0x3f800000;
                        else if (format == VK_FORMAT_R32G32B32A32_SINT)
                            alpha = 0x7fffffff;
                        else
                            alpha = 0xffffffff;

                        if (pDestination == pSource)
                        {
                            auto dPtr = static_cast<uint32_t*>(pDestination);
                            for (size_t count = 0; count < (outSize - 15); count += 16)
                            {
                                dPtr += 3;
                                *(dPtr++) = alpha;
                            }
                        }
                        else
                        {
                            const uint32_t * __restrict sPtr = static_cast<const uint32_t*>(pSource);
                            uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);
                            const size_t size = std::min<size_t>(outSize, inSize);
                            for (size_t count = 0; count < (size - 15); count += 16)
                            {
                                *(dPtr++) = *(sPtr++);
                                *(dPtr++) = *(sPtr++);
                                *(dPtr++) = *(sPtr++);
                                *(dPtr++) = alpha;
                                ++sPtr;
                            }
                        }
                    }
                    return;
                }

                //-----------------------------------------------------------------------------
                case VK_FORMAT_R16G16B16A16_SFLOAT:
                case VK_FORMAT_R16G16B16A16_UNORM:
                case VK_FORMAT_R16G16B16A16_UINT:
                case VK_FORMAT_R16G16B16A16_SNORM:
                case VK_FORMAT_R16G16B16A16_SINT:
                {
                    if (inSize >= 8 && outSize >= 8)
                    {
                        uint16_t alpha;
                        if (format == VK_FORMAT_R16G16B16A16_SFLOAT)
                            alpha = 0x3c00;
                        else if (format == VK_FORMAT_R16G16B16A16_SNORM || format == VK_FORMAT_R16G16B16A16_SINT)
                            alpha = 0x7fff;
                        else
                            alpha = 0xffff;

                        if (pDestination == pSource)
                        {
                            auto dPtr = static_cast<uint16_t*>(pDestination);
                            for (size_t count = 0; count < (outSize - 7); count += 8)
                            {
                                dPtr += 3;
                                *(dPtr++) = alpha;
                            }
                        }
                        else
                        {
                            const uint16_t * __restrict sPtr = static_cast<const uint16_t*>(pSource);
                            uint16_t * __restrict dPtr = static_cast<uint16_t*>(pDestination);
                            const size_t size = std::min<size_t>(outSize, inSize);
                            for (size_t count = 0; count < (size - 7); count += 8)
                            {
                                *(dPtr++) = *(sPtr++);
                                *(dPtr++) = *(sPtr++);
                                *(dPtr++) = *(sPtr++);
                                *(dPtr++) = alpha;
                                ++sPtr;
                            }
                        }
                    }
                    return;
                }

                //-----------------------------------------------------------------------------
                case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
                case VK_FORMAT_A2B10G10R10_UINT_PACK32:
                {
                    if (inSize >= 4 && outSize >= 4)
                    {
                        if (pDestination == pSource)
                        {
                            auto dPtr = static_cast<uint32_t*>(pDestination);
                            for (size_t count = 0; count < (outSize - 3); count += 4)
                            {
                                *dPtr |= 0xC0000000;
                                ++dPtr;
                            }
                        }
                        else
                        {
                            const uint32_t * __restrict sPtr = static_cast<const uint32_t*>(pSource);
                            uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);
                            const size_t size = std::min<size_t>(outSize, inSize);
                            for (size_t count = 0; count < (size - 3); count += 4)
                            {
                                *(dPtr++) = *(sPtr++) | 0xC0000000;
                            }
                        }
                    }
                    return;
                }

                //-----------------------------------------------------------------------------
                case VK_FORMAT_R8G8B8A8_UNORM:
                case VK_FORMAT_R8G8B8A8_SRGB:
                case VK_FORMAT_R8G8B8A8_UINT:
                case VK_FORMAT_R8G8B8A8_SNORM:
                case VK_FORMAT_R8G8B8A8_SINT:
                case VK_FORMAT_B8G8R8A8_UNORM:
                case VK_FORMAT_B8G8R8A8_SRGB:
                {
                    if (inSize >= 4 && outSize >= 4)
                    {
                        const uint32_t alpha = (format == VK_FORMAT_R8G8B8A8_SNORM || format == VK_FORMAT_R8G8B8A8_SINT) ? 0x7f000000 : 0xff000000;

                        if (pDestination == pSource)
                        {
                            auto dPtr = static_cast<uint32_t*>(pDestination);
                            for (size_t count = 0; count < (outSize - 3); count += 4)
                            {
                                uint32_t t = *dPtr & 0xFFFFFF;
                                t |= alpha;
                                *(dPtr++) = t;
                            }
                        }
                        else
                        {
                            const uint32_t * __restrict sPtr = static_cast<const uint32_t*>(pSource);
                            uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);
                            const size_t size = std::min<size_t>(outSize, inSize);
                            for (size_t count = 0; count < (size - 3); count += 4)
                            {
                                uint32_t t = *(sPtr++) & 0xFFFFFF;
                                t |= alpha;
                                *(dPtr++) = t;
                            }
                        }
                    }
                    return;
                }

                //-----------------------------------------------------------------------------
                case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
                case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
                case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
                {
                    if (inSize >= 2 && outSize >= 2)
                    {
                        uint16_t alpha;
                        if (format == VK_FORMAT_A4R4G4B4_UNORM_PACK16)
                            alpha = 0xF000;
                        else if (format == VK_FORMAT_R4G4B4A4_UNORM_PACK16)
                            alpha = 0x000F;
                        else
                            alpha = 0x8000;

                        if (pDestination == pSource)
                        {
                            auto dPtr = static_cast<uint16_t*>(pDestination);
                            for (size_t count = 0; count < (outSize - 1); count += 2)
                            {
                                *(dPtr++) |= alpha;
                            }
                        }
                        else
                        {
                            const uint16_t * __restrict sPtr = static_cast<const uint16_t*>(pSource);
                            uint16_t * __restrict dPtr = static_cast<uint16_t*>(pDestination);
                            const size_t size = std::min<size_t>(outSize, inSize);
                            for (size_t count = 0; count < (size - 1); count += 2)
                            {
                                *(dPtr++) = uint16_t(*(sPtr++) | alpha);
                            }
                        }
                    }
                    return;
                }

                //-----------------------------------------------------------------------------
                case VK_FORMAT_A8_UNORM:
                    memset(pDestination, 0xff, outSize);
                    return;

                default:
                    break;
            }
        }

        // Fall-through case is to just use memcpy (assuming this is not an in-place operation)
        if (pDestination == pSource)
            return;

        const size_t size = std::min<size_t>(outSize, inSize);
        memcpy(pDestination, pSource, size);
    }
}

