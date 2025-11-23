#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vulkan/vulkan_core.h>
#include "VulkanTex.h"
#include "VulkanDDS.h"

using namespace VulkanTex;

static_assert(static_cast<int>(TEX_DIMENSION_TEXTURE1D) == static_cast<int>(DDS_DIMENSION_TEXTURE1D), "header enum mismatch");
static_assert(static_cast<int>(TEX_DIMENSION_TEXTURE2D) == static_cast<int>(DDS_DIMENSION_TEXTURE2D), "header enum mismatch");
static_assert(static_cast<int>(TEX_DIMENSION_TEXTURE3D) == static_cast<int>(DDS_DIMENSION_TEXTURE3D), "header enum mismatch");

namespace
{
    //-------------------------------------------------------------------------------------
    // Legacy format mapping table (used for DDS files without 'DX10' extended header)
    //-------------------------------------------------------------------------------------
    enum CONVERSION_FLAGS : uint32_t
    {
        CONV_FLAGS_NONE = 0x0,
        CONV_FLAGS_EXPAND = 0x1,        // Conversion requires expanded pixel size
        CONV_FLAGS_NOALPHA = 0x2,       // Conversion requires setting alpha to known value
        CONV_FLAGS_SWIZZLE = 0x4,       // BGR/RGB order swizzling required
        CONV_FLAGS_PAL8 = 0x8,          // Has an 8-bit palette
        CONV_FLAGS_888 = 0x10,          // Source is an 8:8:8 (24bpp) format
        CONV_FLAGS_565 = 0x20,          // Source is a 5:6:5 (16bpp) format
        CONV_FLAGS_5551 = 0x40,         // Source is a 5:5:5:1 (16bpp) format
        CONV_FLAGS_4444 = 0x80,         // Source is a 4:4:4:4 (16bpp) format
        CONV_FLAGS_44 = 0x100,          // Source is a 4:4 (8bpp) format
        CONV_FLAGS_332 = 0x200,         // Source is a 3:3:2 (8bpp) format
        CONV_FLAGS_8332 = 0x400,        // Source is a 8:3:3:2 (16bpp) format
        CONV_FLAGS_A8P8 = 0x800,        // Has an 8-bit palette with an alpha channel
        CONF_FLAGS_11ON12 = 0x1000,     // D3D11on12 format
        CONV_FLAGS_DX10 = 0x10000,      // Has the 'DX10' extension header
        CONV_FLAGS_PMALPHA = 0x20000,   // Contains premultiplied alpha data
        CONV_FLAGS_L8 = 0x40000,        // Source is a 8 luminance format
        CONV_FLAGS_L16 = 0x80000,       // Source is a 16 luminance format
        CONV_FLAGS_A8L8 = 0x100000,     // Source is a 8:8 luminance format
        CONV_FLAGS_L6V5U5 = 0x200000,   // Source is a 6:5:5 bumpluminance format
        CONV_FLAGS_L8U8V8 = 0x400000,   // Source is a X:8:8:8 bumpluminance format
        CONV_FLAGS_WUV10 = 0x800000,    // Source is a 2:10:10:10 bump format
    };

    struct LegacyDDS
    {
        VkFormat        format;
        uint32_t        convFlags;
        DDS_PIXELFORMAT ddpf;
    };

    const LegacyDDS g_LegacyDDSMap[] =
    {
        { VK_FORMAT_BC1_RGB_UNORM_BLOCK, CONV_FLAGS_NONE,    DDSPF_DXT1 }, // D3DFMT_DXT1
        { VK_FORMAT_BC2_UNORM_BLOCK,     CONV_FLAGS_NONE,    DDSPF_DXT3 }, // D3DFMT_DXT3
        { VK_FORMAT_BC3_UNORM_BLOCK,     CONV_FLAGS_NONE,    DDSPF_DXT5 }, // D3DFMT_DXT5

        { VK_FORMAT_BC2_UNORM_BLOCK,     CONV_FLAGS_PMALPHA, DDSPF_DXT2 }, // D3DFMT_DXT2
        { VK_FORMAT_BC3_UNORM_BLOCK,     CONV_FLAGS_PMALPHA, DDSPF_DXT4 }, // D3DFMT_DXT4

        // These DXT5 variants have various swizzled channels. They are returned 'as is' to the client as BC3.
        { VK_FORMAT_BC3_UNORM_BLOCK,     CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', '2', 'D', '5'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC3_UNORM_BLOCK,     CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('x', 'G', 'B', 'R'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC3_UNORM_BLOCK,     CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R', 'x', 'B', 'G'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC3_UNORM_BLOCK,     CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R', 'B', 'x', 'G'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC3_UNORM_BLOCK,     CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('x', 'R', 'B', 'G'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC3_UNORM_BLOCK,    CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R', 'G', 'x', 'B'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC3_UNORM_BLOCK,    CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('x', 'G', 'x', 'R'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC3_UNORM_BLOCK,    CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('G', 'X', 'R', 'B'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC3_UNORM_BLOCK,    CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('G', 'R', 'X', 'B'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC3_UNORM_BLOCK,    CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R', 'X', 'G', 'B'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC3_UNORM_BLOCK,    CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B', 'R', 'G', 'X'), 0, 0, 0, 0, 0 } },

        { VK_FORMAT_BC4_UNORM_BLOCK,    CONV_FLAGS_NONE, DDSPF_BC4_UNORM },
        { VK_FORMAT_BC4_SNORM_BLOCK,    CONV_FLAGS_NONE, DDSPF_BC4_SNORM },
        { VK_FORMAT_BC5_UNORM_BLOCK,    CONV_FLAGS_NONE, DDSPF_BC5_UNORM },
        { VK_FORMAT_BC5_SNORM_BLOCK,    CONV_FLAGS_NONE, DDSPF_BC5_SNORM },

        { VK_FORMAT_BC4_UNORM_BLOCK,    CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', 'T', 'I', '1'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC5_UNORM_BLOCK,    CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', 'T', 'I', '2'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC5_UNORM_BLOCK,    CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', '2', 'X', 'Y'), 0, 0, 0, 0, 0 } },

        { VK_FORMAT_BC6H_UFLOAT_BLOCK,  CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B', 'C', '6', 'H'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC7_UNORM_BLOCK,    CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B', 'C', '7', 'L'), 0, 0, 0, 0, 0 } },
        { VK_FORMAT_BC7_UNORM_BLOCK,    CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B', 'C', '7', '\0'), 0, 0, 0, 0, 0 } },


        { VK_FORMAT_B8G8R8A8_UNORM,     CONV_FLAGS_NONE,    DDSPF_A8R8G8B8 }, // D3DFMT_A8R8G8B8 (uses DXGI 1.1 format)
        { VK_FORMAT_B8G8R8A8_UNORM,     CONV_FLAGS_NONE,    DDSPF_X8R8G8B8 }, // D3DFMT_X8R8G8B8 (uses DXGI 1.1 format)
        { VK_FORMAT_R8G8B8A8_UNORM,     CONV_FLAGS_NONE,    DDSPF_A8B8G8R8 }, // D3DFMT_A8B8G8R8
        { VK_FORMAT_R8G8B8A8_UNORM,     CONV_FLAGS_NOALPHA, DDSPF_X8B8G8R8 }, // D3DFMT_X8B8G8R8
        { VK_FORMAT_R16G16_UNORM,       CONV_FLAGS_NONE,    DDSPF_G16R16   }, // D3DFMT_G16R16

        { VK_FORMAT_A2R10G10B10_UNORM_PACK32,  CONV_FLAGS_SWIZZLE, DDSPF_A2R10G10B10 }, // D3DFMT_A2R10G10B10 (D3DX reversal issue)
        { VK_FORMAT_A2R10G10B10_UNORM_PACK32,  CONV_FLAGS_NONE,    DDSPF_A2B10G10R10 }, // D3DFMT_A2B10G10R10 (D3DX reversal issue)

        { VK_FORMAT_R8G8B8A8_UNORM, CONV_FLAGS_EXPAND | CONV_FLAGS_NOALPHA | CONV_FLAGS_888, DDSPF_R8G8B8 }, // D3DFMT_R8G8B8

        { VK_FORMAT_B5G6R5_UNORM_PACK16,   CONV_FLAGS_565,  DDSPF_R5G6B5 }, // D3DFMT_R5G6B5
        { VK_FORMAT_B5G5R5A1_UNORM_PACK16, CONV_FLAGS_5551, DDSPF_A1R5G5B5 }, // D3DFMT_A1R5G5B5
        { VK_FORMAT_B5G5R5A1_UNORM_PACK16, CONV_FLAGS_5551 | CONV_FLAGS_NOALPHA, DDSPF_X1R5G5B5 }, // D3DFMT_X1R5G5B5

        { VK_FORMAT_R8G8B8A8_UNORM,      CONV_FLAGS_EXPAND | CONV_FLAGS_8332, DDSPF_A8R3G3B2 }, // D3DFMT_A8R3G3B2
        { VK_FORMAT_B5G6R5_UNORM_PACK16, CONV_FLAGS_EXPAND | CONV_FLAGS_332, DDSPF_R3G3B2 }, // D3DFMT_R3G3B2

        { VK_FORMAT_R8_UNORM,   CONV_FLAGS_NONE, DDSPF_L8 }, // D3DFMT_L8
        { VK_FORMAT_R16_UNORM,  CONV_FLAGS_NONE, DDSPF_L16 }, // D3DFMT_L16
        { VK_FORMAT_R8G8_UNORM, CONV_FLAGS_NONE, DDSPF_A8L8 }, // D3DFMT_A8L8
        { VK_FORMAT_R8G8_UNORM, CONV_FLAGS_NONE, DDSPF_A8L8_ALT }, // D3DFMT_A8L8 (alternative bitcount)

        // NVTT v1 wrote these with RGB instead of LUMINANCE
        { VK_FORMAT_R8_UNORM,   CONV_FLAGS_NONE, DDSPF_L8_NVTT1 }, // D3DFMT_L8
        { VK_FORMAT_R16_UNORM,  CONV_FLAGS_NONE, DDSPF_L16_NVTT1  }, // D3DFMT_L16
        { VK_FORMAT_R8G8_UNORM, CONV_FLAGS_NONE, DDSPF_A8L8_NVTT1 }, // D3DFMT_A8L8

        { VK_FORMAT_A8_UNORM, CONV_FLAGS_NONE, DDSPF_A8   }, // D3DFMT_A8

        { VK_FORMAT_R16G16B16A16_UNORM,  CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,   36,  0, 0, 0, 0, 0 } }, // D3DFMT_A16B16G16R16
        { VK_FORMAT_R16G16B16A16_SNORM,  CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  110,  0, 0, 0, 0, 0 } }, // D3DFMT_Q16W16V16U16
        { VK_FORMAT_R16_SFLOAT,          CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  111,  0, 0, 0, 0, 0 } }, // D3DFMT_R16F
        { VK_FORMAT_R16G16_SFLOAT,       CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  112,  0, 0, 0, 0, 0 } }, // D3DFMT_G16R16F
        { VK_FORMAT_R16G16B16A16_SFLOAT, CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  113,  0, 0, 0, 0, 0 } }, // D3DFMT_A16B16G16R16F
        { VK_FORMAT_R32_SFLOAT,          CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  114,  0, 0, 0, 0, 0 } }, // D3DFMT_R32F
        { VK_FORMAT_R32G32_SFLOAT,       CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  115,  0, 0, 0, 0, 0 } }, // D3DFMT_G32R32F
        { VK_FORMAT_R32G32B32A32_SFLOAT, CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  116,  0, 0, 0, 0, 0 } }, // D3DFMT_A32B32G32R32F

        { VK_FORMAT_R32_SFLOAT,          CONV_FLAGS_NONE, { sizeof(DDS_PIXELFORMAT), DDS_RGB,       0, 32, 0xffffffff, 0, 0, 0 } }, // D3DFMT_R32F (D3DX uses FourCC 114 instead)

        { VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, CONV_FLAGS_EXPAND | CONV_FLAGS_PAL8 | CONV_FLAGS_A8P8,
                                                            { sizeof(DDS_PIXELFORMAT), DDS_PAL8A, 0, 16, 0, 0, 0, 0xff00 } }, // D3DFMT_A8P8
        { VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, CONV_FLAGS_EXPAND | CONV_FLAGS_PAL8,
                                                            { sizeof(DDS_PIXELFORMAT), DDS_PAL8,  0,  8, 0, 0, 0, 0 } }, // D3DFMT_P8

        { VK_FORMAT_B4G4R4A4_UNORM_PACK16, CONV_FLAGS_4444, DDSPF_A4R4G4B4 }, // D3DFMT_A4R4G4B4 (uses DXGI 1.2 format)
        { VK_FORMAT_B4G4R4A4_UNORM_PACK16, CONV_FLAGS_NOALPHA | CONV_FLAGS_4444,
                                                        DDSPF_X4R4G4B4 }, // D3DFMT_X4R4G4B4 (uses DXGI 1.2 format)
        { VK_FORMAT_B4G4R4A4_UNORM_PACK16, CONV_FLAGS_EXPAND | CONV_FLAGS_44,
                                                        DDSPF_A4L4 }, // D3DFMT_A4L4 (uses DXGI 1.2 format)

        { VK_FORMAT_G8B8G8R8_422_UNORM, CONV_FLAGS_NONE,    DDSPF_YUY2 }, // D3DFMT_YUY2 (uses DXGI 1.2 format)
        { VK_FORMAT_G8B8G8R8_422_UNORM, CONV_FLAGS_SWIZZLE, DDSPF_UYVY }, // D3DFMT_UYVY (uses DXGI 1.2 format)

        { VK_FORMAT_R8G8_SNORM,      CONV_FLAGS_NONE, DDSPF_V8U8 },     // D3DFMT_V8U8
        { VK_FORMAT_R8G8B8A8_SNORM,  CONV_FLAGS_NONE, DDSPF_Q8W8V8U8 }, // D3DFMT_Q8W8V8U8
        { VK_FORMAT_R16G16_SNORM,    CONV_FLAGS_NONE, DDSPF_V16U16 },   // D3DFMT_V16U16

        { VK_FORMAT_R8G8B8A8_UNORM, CONV_FLAGS_L6V5U5 | CONV_FLAGS_EXPAND, DDSPF_L6V5U5 },      // D3DFMT_L6V5U5
        { VK_FORMAT_R8G8B8A8_UNORM, CONV_FLAGS_L8U8V8, DDSPF_X8L8V8U8 },    // D3DFMT_X8L8V8U8
        { VK_FORMAT_A2R10G10B10_UNORM_PACK32, CONV_FLAGS_WUV10, DDSPF_A2W10V10U10 }, // D3DFMT_A2W10V10U10
    };

    // Note that many common DDS reader/writers (including D3DX) swap the
    // the RED/BLUE masks for 10:10:10:2 formats. We assumme
    // below that the 'backwards' header mask is being used since it is most
    // likely written by D3DX. The more robust solution is to use the 'DX10'
    // header extension and specify the DXGI_FORMAT_R10G10B10A2_UNORM format directly

    // We do not support the following legacy Direct3D 9 formats:
    //      D3DFMT_D16_LOCKABLE (DDPF_ZBUFFER: 0x00000400)
    //      FourCC 82 D3DFMT_D32F_LOCKABLE
    //      FourCC 117 D3DFMT_CxV8U8

    // We do not support the following known FourCC codes:
    //      FourCC CTX1 (Xbox 360 only)
    //      FourCC EAR, EARG, ET2, ET2A (Ericsson Texture Compression)
    //      FourCC MET1 (a.k.a. D3DFMT_MULTI2_ARGB8; rarely supported by any hardware)

    VkFormat GetDXGIFormat(const DDS_HEADER& hdr, const DDS_PIXELFORMAT& ddpf,
                           DDS_FLAGS flags, uint32_t& convFlags) noexcept
    {
        uint32_t ddpfFlags = ddpf.flags;

        if (hdr.reserved1[9] == MAKEFOURCC('N', 'V', 'T', 'T'))
        {
            // Clear out non-standard nVidia DDS flags
            ddpfFlags &= ~0xC0000000 /* DDPF_SRGB | DDPF_NORMAL */;
        }

        constexpr size_t MAP_SIZE = sizeof(g_LegacyDDSMap) / sizeof(LegacyDDS);
        size_t           index    = 0;

        if ((ddpf.size == 0) && (ddpf.flags == 0) && (ddpf.fourCC != 0))
        {
            // Handle some DDS files where the DDPF_PIXELFORMAT is mostly zero
            for (index = 0; index < MAP_SIZE; ++index)
            {
                const LegacyDDS* entry = &g_LegacyDDSMap[index];

                if (entry->ddpf.flags & DDS_FOURCC)
                {
                    if (ddpf.fourCC == entry->ddpf.fourCC)
                        break;
                }
            }
        }
        else
        {
            for (index = 0; index < MAP_SIZE; ++index)
            {
                const LegacyDDS* entry = &g_LegacyDDSMap[index];

                if ((ddpfFlags & DDS_FOURCC) && (entry->ddpf.flags & DDS_FOURCC))
                {
                    // In case of FourCC codes, ignore any other bits in ddpf.flags
                    if (ddpf.fourCC == entry->ddpf.fourCC)
                        break;
                }
                else if ((ddpfFlags == entry->ddpf.flags) && (ddpf.RGBBitCount == entry->ddpf.RGBBitCount))
                {
                    if (entry->ddpf.flags & DDS_PAL8)
                    {
                        // PAL8 / PAL8A
                        break;
                    }
                    else if (entry->ddpf.flags & DDS_ALPHA)
                    {
                        if (ddpf.ABitMask == entry->ddpf.ABitMask)
                            break;
                    }
                    else if (entry->ddpf.flags & DDS_LUMINANCE)
                    {
                        if (entry->ddpf.flags & DDS_ALPHAPIXELS)
                        {
                            // LUMINANCEA
                            if ((ddpf.RBitMask == entry->ddpf.RBitMask) &&
                                (ddpf.ABitMask == entry->ddpf.ABitMask))
                                break;
                        }
                        else
                        {
                            // LUMINANCE
                            if (ddpf.RBitMask == entry->ddpf.RBitMask)
                                break;
                        }
                    }
                    else if (entry->ddpf.flags & DDS_BUMPDUDV)
                    {
                        if (entry->ddpf.flags & DDS_ALPHAPIXELS)
                        {
                            // BUMPDUDVA
                            if ((ddpf.RBitMask == entry->ddpf.RBitMask) &&
                                (ddpf.ABitMask == entry->ddpf.ABitMask))
                            {
                                flags &= ~DDS_FLAGS_NO_R10B10G10A2_FIXUP;
                                break;
                            }
                        }
                        else
                        {
                            // BUMPDUDV
                            if (ddpf.RBitMask == entry->ddpf.RBitMask)
                                break;
                        }
                    }
                    else if (entry->ddpf.flags & DDS_ALPHAPIXELS)
                    {
                        // RGBA
                        if ((ddpf.RBitMask == entry->ddpf.RBitMask) &&
                            (ddpf.GBitMask == entry->ddpf.GBitMask) &&
                            (ddpf.BBitMask == entry->ddpf.BBitMask) &&
                            (ddpf.ABitMask == entry->ddpf.ABitMask))
                            break;
                    }
                    else
                    {
                        // RGB
                        if ((ddpf.RBitMask == entry->ddpf.RBitMask) &&
                            (ddpf.GBitMask == entry->ddpf.GBitMask) &&
                            (ddpf.BBitMask == entry->ddpf.BBitMask))
                            break;
                    }
                }
            }
        }

        if (index >= MAP_SIZE)
            return VK_FORMAT_UNDEFINED;

        uint32_t cflags = g_LegacyDDSMap[index].convFlags;
        VkFormat format = g_LegacyDDSMap[index].format;

        if ((cflags & CONV_FLAGS_EXPAND) && (flags & DDS_FLAGS_NO_LEGACY_EXPANSION))
            return VK_FORMAT_UNDEFINED;

        if ((format == VK_FORMAT_A2R10G10B10_UNORM_PACK32) && (flags & DDS_FLAGS_NO_R10B10G10A2_FIXUP))
        {
            cflags ^= CONV_FLAGS_SWIZZLE;
        }

        if ((hdr.reserved1[9] == MAKEFOURCC('N', 'V', 'T', 'T')) &&
            (ddpf.flags & 0x40000000 /* DDPF_SRGB */))
        {
            format = MakeSRGB(format);
        }

        convFlags = cflags;

        return format;
    }

    //-------------------------------------------------------------------------------------
    // Decodes DDS header including optional DX10 extended header
    //-------------------------------------------------------------------------------------
    bool DecodeDDSHeader(
        const void* pSource,
        size_t size,
        DDS_FLAGS flags,
        TexMetadata& metadata,
        DDSMetaData* ddPixelFormat,
        uint32_t& convFlags) noexcept
    {
        if (!pSource)
            return false;

        metadata = {};
        if (ddPixelFormat)
        {
            *ddPixelFormat = {};
        }

        if (size < DDS_MIN_HEADER_SIZE)
        {
            return false;
        }

        // DDS files always start with the same magic number ("DDS ")
        const auto dwMagicNumber = *static_cast<const uint32_t*>(pSource);
        if (dwMagicNumber != DDS_MAGIC)
        {
            return false;
        }

        auto pHeader = reinterpret_cast<const DDS_HEADER*>(static_cast<const uint8_t*>(pSource) + sizeof(uint32_t));

        // Verify header to validate DDS file
        if (flags & DDS_FLAGS_PERMISSIVE)
        {
            if (pHeader->size != 24 /* Known variant */
                && pHeader->size != sizeof(DDS_HEADER))
            {
                return false;
            }
        }
        else if (pHeader->size != sizeof(DDS_HEADER))
        {
            return false;
        }

        if (flags & DDS_FLAGS_PERMISSIVE)
        {
            if (pHeader->ddspf.size != 0 /* Known variant */
                && pHeader->ddspf.size != 24 /* Known variant */
                && pHeader->ddspf.size != sizeof(DDS_PIXELFORMAT))
            {
                return false;
            }
        }
        else if (pHeader->ddspf.size != sizeof(DDS_PIXELFORMAT))
        {
            return false;
        }

        metadata.mipLevels = pHeader->mipMapCount;

        if (metadata.mipLevels == 0)
        {
            metadata.mipLevels = 1;
        }

        // Check for DX10 extension
        if ((pHeader->ddspf.flags & DDS_FOURCC) &&
            (MAKEFOURCC('D', 'X', '1', '0') == pHeader->ddspf.fourCC))
        {
            if ((pHeader->size       != sizeof(DDS_HEADER)) ||
                (pHeader->ddspf.size != sizeof(DDS_PIXELFORMAT)))
            {
                // We do not accept legacy DX9 'known variants' for modern "DX10" extension header files.
                return false;
            }

            // Buffer must be big enough for both headers and magic value
            if (size < DDS_DX10_HEADER_SIZE)
            {
                return false;
            }

            auto d3d10ext = reinterpret_cast<const DDS_HEADER_DXT10*>(static_cast<const uint8_t*>(pSource) + DDS_MIN_HEADER_SIZE);
            convFlags |= CONV_FLAGS_DX10;

            metadata.arraySize = d3d10ext->arraySize;

            if (metadata.arraySize == 0)
            {
                metadata.arraySize = 1;
            }

            metadata.format = d3d10ext->dxgiFormat;

            if (!IsValid(metadata.format) || IsPalettized(metadata.format))
            {
                return false;
            }

            static_assert(static_cast<int>(TEX_MISC_TEXTURECUBE) == static_cast<int>(DDS_RESOURCE_MISC_TEXTURECUBE), "DDS header mismatch");

            metadata.miscFlags = d3d10ext->miscFlag & ~static_cast<uint32_t>(TEX_MISC_TEXTURECUBE);

            switch (d3d10ext->resourceDimension)
            {
                case DDS_DIMENSION_TEXTURE1D:
                {
                    // D3DX writes 1D textures with a fixed Height of 1
                    if ((pHeader->flags & DDS_HEIGHT) && pHeader->height != 1)
                    {
                        return false;
                    }

                    metadata.width     = pHeader->width;
                    metadata.height    = 1;
                    metadata.depth     = 1;
                    metadata.dimension = TEX_DIMENSION_TEXTURE1D;
                    break;
                }

                case DDS_DIMENSION_TEXTURE2D:
                {
                    if (d3d10ext->miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE)
                    {
                        metadata.miscFlags |= TEX_MISC_TEXTURECUBE;
                        metadata.arraySize *= 6;
                    }

                    metadata.width     = pHeader->width;
                    metadata.height    = pHeader->height;
                    metadata.depth     = 1;
                    metadata.dimension = TEX_DIMENSION_TEXTURE2D;
                    break;
                }

                case DDS_DIMENSION_TEXTURE3D:
                {
                    if (!(pHeader->flags & DDS_HEADER_FLAGS_VOLUME))
                    {
                        return false;
                    }

                    if (metadata.arraySize > 1)
                        return false;

                    metadata.width     = pHeader->width;
                    metadata.height    = pHeader->height;
                    metadata.depth     = pHeader->depth;
                    metadata.dimension = TEX_DIMENSION_TEXTURE3D;
                    break;
                }

                default:
                    return false;
            }

            static_assert(static_cast<int>(TEX_MISC2_ALPHA_MODE_MASK) == static_cast<int>(DDS_MISC_FLAGS2_ALPHA_MODE_MASK), "DDS header mismatch");

            static_assert(static_cast<int>(TEX_ALPHA_MODE_UNKNOWN) == static_cast<int>(DDS_ALPHA_MODE_UNKNOWN), "DDS header mismatch");
            static_assert(static_cast<int>(TEX_ALPHA_MODE_STRAIGHT) == static_cast<int>(DDS_ALPHA_MODE_STRAIGHT), "DDS header mismatch");
            static_assert(static_cast<int>(TEX_ALPHA_MODE_PREMULTIPLIED) == static_cast<int>(DDS_ALPHA_MODE_PREMULTIPLIED), "DDS header mismatch");
            static_assert(static_cast<int>(TEX_ALPHA_MODE_OPAQUE) == static_cast<int>(DDS_ALPHA_MODE_OPAQUE), "DDS header mismatch");
            static_assert(static_cast<int>(TEX_ALPHA_MODE_CUSTOM) == static_cast<int>(DDS_ALPHA_MODE_CUSTOM), "DDS header mismatch");

            metadata.miscFlags2 = d3d10ext->miscFlags2;
        }
        else
        {
            metadata.arraySize = 1;

            if (pHeader->flags & DDS_HEADER_FLAGS_VOLUME)
            {
                metadata.width = pHeader->width;
                metadata.height = pHeader->height;
                metadata.depth = pHeader->depth;
                metadata.dimension = TEX_DIMENSION_TEXTURE3D;

                if (flags & DDS_FLAGS_PERMISSIVE)
                {
                    // Allow cases where mipCount was computed incorrectly
                    size_t maxMips = 0;
                    std::ignore = CalculateMipLevels3D(metadata.width, metadata.height, metadata.depth, maxMips);
                    metadata.mipLevels = std::min(metadata.mipLevels, maxMips);
                }
            }
            else
            {
                if (pHeader->caps2 & DDS_CUBEMAP)
                {
                    // We require all six faces to be defined
                    if ((pHeader->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES)
                        return false;

                    metadata.arraySize = 6;
                    metadata.miscFlags |= TEX_MISC_TEXTURECUBE;
                }

                metadata.width = pHeader->width;
                metadata.height = pHeader->height;
                metadata.depth = 1;
                metadata.dimension = TEX_DIMENSION_TEXTURE2D;

                // Note there's no way for a legacy Direct3D 9 DDS to express a '1D' texture

                if (flags & DDS_FLAGS_PERMISSIVE)
                {
                    // Allow cases where mipCount was computed incorrectly
                    size_t maxMips = 0;
                    std::ignore = CalculateMipLevels(metadata.width, metadata.height, maxMips);
                    metadata.mipLevels = std::min(metadata.mipLevels, maxMips);
                }
            }

            metadata.format = GetDXGIFormat(*pHeader, pHeader->ddspf, flags, convFlags);

            if (metadata.format == VK_FORMAT_UNDEFINED)
                return false;

            // Special flag for handling LUMINANCE legacy formats
            if (flags & DDS_FLAGS_EXPAND_LUMINANCE)
            {
                switch (metadata.format)
                {
                    case VK_FORMAT_R8_UNORM:
                        metadata.format = VK_FORMAT_R8G8B8A8_UNORM;
                        convFlags |= CONV_FLAGS_L8 | CONV_FLAGS_EXPAND;
                        break;

                    case VK_FORMAT_R8G8_UNORM:
                        metadata.format = VK_FORMAT_R8G8B8A8_UNORM;
                        convFlags |= CONV_FLAGS_A8L8 | CONV_FLAGS_EXPAND;
                        break;

                    case VK_FORMAT_R16_UNORM:
                        metadata.format = VK_FORMAT_R16G16B16A16_UNORM;
                        convFlags |= CONV_FLAGS_L16 | CONV_FLAGS_EXPAND;
                        break;

                    default:
                        break;
                }
            }
        }

        // Special flag for handling BGR DXGI 1.1 formats
        if (flags & DDS_FLAGS_FORCE_RGB)
        {
            switch (metadata.format)
            {
                case VK_FORMAT_B8G8R8A8_UNORM:
                    metadata.format = VK_FORMAT_R8G8B8A8_UNORM;
                    convFlags |= CONV_FLAGS_SWIZZLE;
                    break;

                case VK_FORMAT_B8G8R8_UNORM:
                    metadata.format = VK_FORMAT_R8G8B8A8_UNORM;
                    convFlags |= CONV_FLAGS_SWIZZLE | CONV_FLAGS_NOALPHA;
                    break;

                case VK_FORMAT_B8G8R8A8_SRGB:
                    metadata.format = VK_FORMAT_R8G8B8A8_SRGB;
                    convFlags |= CONV_FLAGS_SWIZZLE;
                    break;

                case VK_FORMAT_B8G8R8_SRGB:
                    metadata.format = VK_FORMAT_R8G8B8A8_SRGB;
                    convFlags |= CONV_FLAGS_SWIZZLE | CONV_FLAGS_NOALPHA;
                    break;

                default:
                    break;
            }
        }

        // Special flag for handling 16bpp formats
        if (flags & DDS_FLAGS_NO_16BPP)
        {
            switch (static_cast<int>(metadata.format))
            {
                case VK_FORMAT_B5G6R5_UNORM_PACK16:
                case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
                case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
                case VK_FORMAT_A4B4G4R4_UNORM_PACK16:
                    if (metadata.format == VK_FORMAT_B5G6R5_UNORM_PACK16)
                    {
                        convFlags |= CONV_FLAGS_NOALPHA;
                    }
                    if (metadata.format == VK_FORMAT_A4B4G4R4_UNORM_PACK16)
                    {
                        convFlags |= CONV_FLAGS_4444 | CONF_FLAGS_11ON12;
                    }
                    metadata.format = VK_FORMAT_R8G8B8A8_UNORM;
                    convFlags |= CONV_FLAGS_EXPAND;
                    break;

                default:
                    break;
            }
        }

        // Implicit alpha mode
        if (convFlags & CONV_FLAGS_NOALPHA)
        {
            metadata.SetAlphaMode(TEX_ALPHA_MODE_OPAQUE);
        }
        else if (convFlags & CONV_FLAGS_PMALPHA)
        {
            metadata.SetAlphaMode(TEX_ALPHA_MODE_PREMULTIPLIED);
        }

        // Check for .dds files that exceed known hardware support
        if (!(flags & DDS_FLAGS_ALLOW_LARGE_FILES))
        {
            // 16k is the maximum required resource size supported by Direct3D
            if (metadata.width > 16384u /* D3D12_REQ_TEXTURE1D_U_DIMENSION, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION */
                || metadata.height > 16384u /* D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION */
                || metadata.mipLevels > 15u /* D3D12_REQ_MIP_LEVELS */)
            {
                return false;
            }

            // 2048 is the maximum required depth/array size supported by Direct3D
            if (metadata.arraySize > 2048u /* D3D12_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION, D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION */
                || metadata.depth > 2048u /* D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION */)
            {
                return false;
            }
        }

        // Special-handling flag for ignoring mipchains on simple DDS files
        if ((flags & DDS_FLAGS_IGNORE_MIPS) && (metadata.arraySize == 1))
        {
            metadata.mipLevels = 1;
        }

        // Handle DDS-specific metadata
        if (ddPixelFormat)
        {
            ddPixelFormat->size = pHeader->ddspf.size;
            ddPixelFormat->flags = pHeader->ddspf.flags;
            ddPixelFormat->fourCC = pHeader->ddspf.fourCC;
            ddPixelFormat->RGBBitCount = pHeader->ddspf.RGBBitCount;
            ddPixelFormat->RBitMask = pHeader->ddspf.RBitMask;
            ddPixelFormat->GBitMask = pHeader->ddspf.GBitMask;
            ddPixelFormat->BBitMask = pHeader->ddspf.BBitMask;
            ddPixelFormat->ABitMask = pHeader->ddspf.ABitMask;
        }

        return true;
    }

    inline void CopyScanline24bpp(
        uint8_t* pDestination,
        const uint8_t* pSource,
        size_t width) noexcept
    {
        for (size_t x = 0; x < width; ++x)
        {
            pDestination[0] = pSource[0]; // B
            pDestination[1] = pSource[1]; // G
            pDestination[2] = pSource[2]; // R

            pSource += 4;
            pDestination += 3;
        }
    }
}


//-------------------------------------------------------------------------------------
// Encodes DDS file header (magic value, header, optional DX10 extended header)
//-------------------------------------------------------------------------------------
bool VulkanTex::EncodeDDSHeader(
    const TexMetadata& metadata,
    DDS_FLAGS flags,
    uint8_t* pDestination,
    size_t maxsize,
    size_t& required) noexcept
{
    if (!IsValid(metadata.format))
        return false;

    if (metadata.arraySize > 1)
    {
        if ((metadata.arraySize != 6) || (metadata.dimension != TEX_DIMENSION_TEXTURE2D) || !(metadata.IsCubemap()))
        {
            // Texture1D arrays, Texture2D arrays, and Cubemap arrays must be stored using 'DX10' extended header
            if (flags & DDS_FLAGS_FORCE_DX9_LEGACY)
                return false;

            flags |= DDS_FLAGS_FORCE_DX10_EXT;
        }
    }

    if (flags & DDS_FLAGS_FORCE_DX10_EXT_MISC2)
    {
        flags |= DDS_FLAGS_FORCE_DX10_EXT;
    }

    CP_FLAGS pitchFlags  = CP_FLAGS_NONE;
    DDS_PIXELFORMAT ddpf = {};

    if (!(flags & DDS_FLAGS_FORCE_DX10_EXT))
    {
        switch (metadata.format)
        {
            case VK_FORMAT_R8G8B8A8_UNORM:        memcpy(&ddpf, &DDSPF_A8B8G8R8, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_R16G16_UNORM:          memcpy(&ddpf, &DDSPF_G16R16, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_R8G8_UNORM:            memcpy(&ddpf, &DDSPF_A8L8, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_R16_UNORM:             memcpy(&ddpf, &DDSPF_L16, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_R8_UNORM:              memcpy(&ddpf, &DDSPF_L8, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_A8_UNORM:              memcpy(&ddpf, &DDSPF_A8, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_B8G8R8G8_422_UNORM:    memcpy(&ddpf, &DDSPF_R8G8_B8G8, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_G8B8G8R8_422_UNORM:    memcpy(&ddpf, &DDSPF_G8R8_G8B8, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:   memcpy(&ddpf, &DDSPF_DXT1, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_BC2_UNORM_BLOCK:       memcpy(&ddpf, metadata.IsPMAlpha() ? (&DDSPF_DXT2) : (&DDSPF_DXT3), sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_BC4_SNORM_BLOCK:       memcpy(&ddpf, &DDSPF_BC4_SNORM, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_BC5_SNORM_BLOCK:       memcpy(&ddpf, &DDSPF_BC5_SNORM, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_B5G6R5_UNORM_PACK16:   memcpy(&ddpf, &DDSPF_R5G6B5, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_B5G5R5A1_UNORM_PACK16: memcpy(&ddpf, &DDSPF_A1R5G5B5, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_R8G8_SNORM:            memcpy(&ddpf, &DDSPF_V8U8, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_R8G8B8A8_SNORM:        memcpy(&ddpf, &DDSPF_Q8W8V8U8, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_R16G16_SNORM:          memcpy(&ddpf, &DDSPF_V16U16, sizeof(DDS_PIXELFORMAT)); break;
            case VK_FORMAT_B8G8R8A8_UNORM:        memcpy(&ddpf, &DDSPF_A8R8G8B8, sizeof(DDS_PIXELFORMAT)); break; // DXGI 1.1
            case VK_FORMAT_B8G8R8_UNORM:
            {
                if (flags & DDS_FLAGS_FORCE_24BPP_RGB)
                {

                    memcpy(&ddpf, &DDSPF_R8G8B8, sizeof(DDS_PIXELFORMAT)); // No DXGI equivalent
                    pitchFlags |= CP_FLAGS_24BPP;
                }
                else
                {
                    memcpy(&ddpf, &DDSPF_X8R8G8B8, sizeof(DDS_PIXELFORMAT)); // DXGI 1.1
                }

                break;
            }
            case VK_FORMAT_B4G4R4A4_UNORM_PACK16: memcpy(&ddpf, &DDSPF_A4R4G4B4, sizeof(DDS_PIXELFORMAT)); break; // DXGI 1.2

            case VK_FORMAT_BC3_UNORM_BLOCK:
            {
                memcpy(&ddpf, metadata.IsPMAlpha() ? (&DDSPF_DXT4) : (&DDSPF_DXT5), sizeof(DDS_PIXELFORMAT));
                if (flags & DDS_FLAGS_FORCE_DXT5_RXGB)
                {
                    ddpf.fourCC = MAKEFOURCC('R', 'X', 'G', 'B');
                }
                break;
            }

            // Legacy D3DX formats using D3DFMT enum value as FourCC
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                ddpf.size = sizeof(DDS_PIXELFORMAT); ddpf.flags = DDS_FOURCC; ddpf.fourCC = 116;  // D3DFMT_A32B32G32R32F
                break;
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                ddpf.size = sizeof(DDS_PIXELFORMAT); ddpf.flags = DDS_FOURCC; ddpf.fourCC = 113;  // D3DFMT_A16B16G16R16F
                break;
            case VK_FORMAT_R16G16B16A16_UNORM:
                ddpf.size = sizeof(DDS_PIXELFORMAT); ddpf.flags = DDS_FOURCC; ddpf.fourCC = 36;  // D3DFMT_A16B16G16R16
                break;
            case VK_FORMAT_R16G16B16A16_SNORM:
                ddpf.size = sizeof(DDS_PIXELFORMAT); ddpf.flags = DDS_FOURCC; ddpf.fourCC = 110;  // D3DFMT_Q16W16V16U16
                break;
            case VK_FORMAT_R32G32_SFLOAT:
                ddpf.size = sizeof(DDS_PIXELFORMAT); ddpf.flags = DDS_FOURCC; ddpf.fourCC = 115;  // D3DFMT_G32R32F
                break;
            case VK_FORMAT_R16G16_SFLOAT:
                ddpf.size = sizeof(DDS_PIXELFORMAT); ddpf.flags = DDS_FOURCC; ddpf.fourCC = 112;  // D3DFMT_G16R16F
                break;
            case VK_FORMAT_R32_SFLOAT:
                ddpf.size = sizeof(DDS_PIXELFORMAT); ddpf.flags = DDS_FOURCC; ddpf.fourCC = 114;  // D3DFMT_R32F
                break;
            case VK_FORMAT_R16_SFLOAT:
                ddpf.size = sizeof(DDS_PIXELFORMAT); ddpf.flags = DDS_FOURCC; ddpf.fourCC = 111;  // D3DFMT_R16F
                break;

            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            {
                if (flags & DDS_FLAGS_FORCE_DX9_LEGACY)
                {
                    // Write using the 'incorrect' mask version to match D3DX bug
                    memcpy(&ddpf, &DDSPF_A2B10G10R10, sizeof(DDS_PIXELFORMAT));
                }
                break;
            }

            case VK_FORMAT_R8G8B8A8_SRGB:
            {
                if (flags & DDS_FLAGS_FORCE_DX9_LEGACY)
                {
                    memcpy(&ddpf, &DDSPF_A8B8G8R8, sizeof(DDS_PIXELFORMAT));
                }
                break;
            }

            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            {
                if (flags & DDS_FLAGS_FORCE_DX9_LEGACY)
                {
                    memcpy(&ddpf, &DDSPF_DXT1, sizeof(DDS_PIXELFORMAT));
                }
                break;
            }

            case VK_FORMAT_BC2_SRGB_BLOCK:
            {
                if (flags & DDS_FLAGS_FORCE_DX9_LEGACY)
                {
                    memcpy(&ddpf, metadata.IsPMAlpha() ? (&DDSPF_DXT2) : (&DDSPF_DXT3), sizeof(DDS_PIXELFORMAT));
                }
                break;
            }

            case VK_FORMAT_BC3_SRGB_BLOCK:
            {
                if (flags & DDS_FLAGS_FORCE_DX9_LEGACY)
                {
                    memcpy(&ddpf, metadata.IsPMAlpha() ? (&DDSPF_DXT4) : (&DDSPF_DXT5), sizeof(DDS_PIXELFORMAT));
                }
                break;
            }

            case VK_FORMAT_BC4_UNORM_BLOCK:
            {
                memcpy(&ddpf, &DDSPF_BC4_UNORM, sizeof(DDS_PIXELFORMAT));
                if (flags & DDS_FLAGS_FORCE_DX9_LEGACY)
                {
                    ddpf.fourCC = MAKEFOURCC('A', 'T', 'I', '1');
                }
                break;
            }

            case VK_FORMAT_BC5_UNORM_BLOCK:
            {
                memcpy(&ddpf, &DDSPF_BC5_UNORM, sizeof(DDS_PIXELFORMAT));
                if (flags & DDS_FLAGS_FORCE_DX9_LEGACY)
                {
                    ddpf.fourCC = MAKEFOURCC('A', 'T', 'I', '2');
                }
                break;
            }

            case VK_FORMAT_B8G8R8A8_SRGB:
            {
                if (flags & DDS_FLAGS_FORCE_DX9_LEGACY)
                {
                    memcpy(&ddpf, &DDSPF_A8R8G8B8, sizeof(DDS_PIXELFORMAT));
                }
                break;
            }

            case VK_FORMAT_B8G8R8_SRGB:
            {
                if (flags & DDS_FLAGS_FORCE_DX9_LEGACY)
                {
                    memcpy(&ddpf, &DDSPF_X8R8G8B8, sizeof(DDS_PIXELFORMAT));
                }
                break;
            }

            default:
                break;
        }
    }

    required = DDS_MIN_HEADER_SIZE;

    if (ddpf.size == 0)
    {
        if (flags & DDS_FLAGS_FORCE_DX9_LEGACY)
            return false;

        required += sizeof(DDS_HEADER_DXT10);
    }

    if (!pDestination)
        return true;

    if (maxsize < required)
        return false;

    *reinterpret_cast<uint32_t*>(pDestination) = DDS_MAGIC;

    DDS_HEADER* header = reinterpret_cast<DDS_HEADER*>(static_cast<uint8_t*>(pDestination) + sizeof(uint32_t));
    assert(header);

    memset(header, 0, sizeof(DDS_HEADER));
    header->size  = sizeof(DDS_HEADER);
    header->flags = DDS_HEADER_FLAGS_TEXTURE;
    header->caps  = DDS_SURFACE_FLAGS_TEXTURE;

    if (metadata.mipLevels > 0)
    {
        header->flags |= DDS_HEADER_FLAGS_MIPMAP;

        if (metadata.mipLevels > UINT16_MAX)
            return false;

        header->mipMapCount = static_cast<uint32_t>(metadata.mipLevels);

        if (header->mipMapCount > 1)
            header->caps |= DDS_SURFACE_FLAGS_MIPMAP;
    }

    switch (metadata.dimension)
    {
        case TEX_DIMENSION_TEXTURE1D:
        {
            if (metadata.width > UINT32_MAX)
                return false;

            header->width  = static_cast<uint32_t>(metadata.width);
            header->height = header->depth = 1;
            break;
        }

        case TEX_DIMENSION_TEXTURE2D:
        {
            if ((metadata.height > UINT32_MAX) || (metadata.width > UINT32_MAX))
                return false;

            header->height = static_cast<uint32_t>(metadata.height);
            header->width  = static_cast<uint32_t>(metadata.width);
            header->depth  = 1;

            if (metadata.IsCubemap())
            {
                header->caps  |= DDS_SURFACE_FLAGS_CUBEMAP;
                header->caps2 |= DDS_CUBEMAP_ALLFACES;
            }
            break;
        }

        case TEX_DIMENSION_TEXTURE3D:
        {
            if ((metadata.height > UINT32_MAX) || 
                (metadata.width > UINT32_MAX)  ||
                (metadata.depth > UINT16_MAX))
                return false;

            header->flags |= DDS_HEADER_FLAGS_VOLUME;
            header->caps2 |= DDS_FLAGS_VOLUME;
            header->height = static_cast<uint32_t>(metadata.height);
            header->width  = static_cast<uint32_t>(metadata.width);
            header->depth  = static_cast<uint32_t>(metadata.depth);
            break;
        }

        default:
            return false;
    }

    size_t rowPitch, slicePitch;
    bool hr = ComputePitch(metadata.format,
                    metadata.width, metadata.height,
                        rowPitch, slicePitch, pitchFlags);

    if (hr == false)
        return hr;

    if ((slicePitch > UINT32_MAX) || (rowPitch > UINT32_MAX))
        return false;

    if (IsCompressed(metadata.format))
    {
        header->flags |= DDS_HEADER_FLAGS_LINEARSIZE;
        header->pitchOrLinearSize = static_cast<uint32_t>(slicePitch);
    }
    else
    {
        header->flags |= DDS_HEADER_FLAGS_PITCH;
        header->pitchOrLinearSize = static_cast<uint32_t>(rowPitch);
    }

    if (ddpf.size == 0)
    {
        memcpy(&header->ddspf, &DDSPF_DX10, sizeof(DDS_PIXELFORMAT));

        auto ext = reinterpret_cast<DDS_HEADER_DXT10*>(reinterpret_cast<uint8_t*>(header) + sizeof(DDS_HEADER));
        assert(ext);

        memset(ext, 0, sizeof(DDS_HEADER_DXT10));
        ext->dxgiFormat = metadata.format;
        ext->resourceDimension = metadata.dimension;

        if (metadata.arraySize > UINT16_MAX)
            return false;

        static_assert(static_cast<int>(TEX_MISC_TEXTURECUBE) == static_cast<int>(DDS_RESOURCE_MISC_TEXTURECUBE), "DDS header mismatch");

        ext->miscFlag = metadata.miscFlags & ~static_cast<uint32_t>(TEX_MISC_TEXTURECUBE);

        if (metadata.miscFlags & TEX_MISC_TEXTURECUBE)
        {
            ext->miscFlag |= TEX_MISC_TEXTURECUBE;

            if ((metadata.arraySize % 6) != 0)
                return false;

            ext->arraySize = static_cast<uint32_t>(metadata.arraySize / 6);
        }
        else
        {
            ext->arraySize = static_cast<uint32_t>(metadata.arraySize);
        }

        static_assert(static_cast<int>(TEX_MISC2_ALPHA_MODE_MASK) == static_cast<int>(DDS_MISC_FLAGS2_ALPHA_MODE_MASK), "DDS header mismatch");

        static_assert(static_cast<int>(TEX_ALPHA_MODE_UNKNOWN) == static_cast<int>(DDS_ALPHA_MODE_UNKNOWN), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_STRAIGHT) == static_cast<int>(DDS_ALPHA_MODE_STRAIGHT), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_PREMULTIPLIED) == static_cast<int>(DDS_ALPHA_MODE_PREMULTIPLIED), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_OPAQUE) == static_cast<int>(DDS_ALPHA_MODE_OPAQUE), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_CUSTOM) == static_cast<int>(DDS_ALPHA_MODE_CUSTOM), "DDS header mismatch");

        if (flags & DDS_FLAGS_FORCE_DX10_EXT_MISC2)
        {
            // This was formerly 'reserved'. D3DX10 and D3DX11 will fail if this value is anything other than 0
            ext->miscFlags2 = metadata.miscFlags2;
        }
    }
    else
    {
        memcpy(&header->ddspf, &ddpf, sizeof(ddpf));
    }

    return true;
}

namespace
{
    //-------------------------------------------------------------------------------------
    // Converts an image row with optional clearing of alpha value to 1.0
    // Returns true if supported, false if expansion case not supported
    //-------------------------------------------------------------------------------------
    enum TEXP_LEGACY_FORMAT
    {
        TEXP_LEGACY_UNKNOWN = 0,
        TEXP_LEGACY_R8G8B8,
        TEXP_LEGACY_R3G3B2,
        TEXP_LEGACY_A8R3G3B2,
        TEXP_LEGACY_P8,
        TEXP_LEGACY_A8P8,
        TEXP_LEGACY_A4L4,
        TEXP_LEGACY_B4G4R4A4,
        TEXP_LEGACY_L8,
        TEXP_LEGACY_L16,
        TEXP_LEGACY_A8L8,
        TEXP_LEGACY_L6V5U5,
        TEXP_LEGACY_X8L8V8U8,
        TEXP_LEGACY_A2W10V10U10
    };

    constexpr TEXP_LEGACY_FORMAT FindLegacyFormat(uint32_t flags) noexcept
    {
        TEXP_LEGACY_FORMAT lformat = TEXP_LEGACY_UNKNOWN;

        if (flags & CONV_FLAGS_PAL8)
        {
            lformat = (flags & CONV_FLAGS_A8P8) ? TEXP_LEGACY_A8P8 : TEXP_LEGACY_P8;
        }
        else if (flags & CONV_FLAGS_888)
            lformat = TEXP_LEGACY_R8G8B8;
        else if (flags & CONV_FLAGS_332)
            lformat = TEXP_LEGACY_R3G3B2;
        else if (flags & CONV_FLAGS_8332)
            lformat = TEXP_LEGACY_A8R3G3B2;
        else if (flags & CONV_FLAGS_44)
            lformat = TEXP_LEGACY_A4L4;
        else if (flags & CONV_FLAGS_4444)
            lformat = TEXP_LEGACY_B4G4R4A4;
        else if (flags & CONV_FLAGS_L8)
            lformat = TEXP_LEGACY_L8;
        else if (flags & CONV_FLAGS_L16)
            lformat = TEXP_LEGACY_L16;
        else if (flags & CONV_FLAGS_A8L8)
            lformat = TEXP_LEGACY_A8L8;
        else if (flags & CONV_FLAGS_L6V5U5)
            lformat = TEXP_LEGACY_L6V5U5;
        else if (flags & CONV_FLAGS_L8U8V8)
            lformat = TEXP_LEGACY_X8L8V8U8;
        else if (flags & CONV_FLAGS_WUV10)
            lformat = TEXP_LEGACY_A2W10V10U10;

        return lformat;
    }

    bool LegacyExpandScanline(
        void* pDestination,
        size_t outSize,
        VkFormat outFormat,
        const void* pSource,
        size_t inSize,
        TEXP_LEGACY_FORMAT inFormat,
        const uint32_t* pal8,
        uint32_t tflags) noexcept
    {
        assert(pDestination && outSize > 0);
        assert(pSource && inSize > 0);
        assert(IsValid(outFormat) && !IsPlanar(outFormat) && !IsPalettized(outFormat));

        switch (inFormat)
        {
        case TEXP_LEGACY_R8G8B8:
            if (outFormat != VK_FORMAT_R8G8B8A8_UNORM)
                return false;

            // D3DFMT_R8G8B8 -> DXGI_FORMAT_R8G8B8A8_UNORM
            if (inSize >= 3 && outSize >= 4)
            {
                const uint8_t * __restrict sPtr = static_cast<const uint8_t*>(pSource);
                uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);

                for (size_t ocount = 0, icount = 0; ((icount < (inSize - 2)) && (ocount < (outSize - 3))); icount += 3, ocount += 4)
                {
                    // 24bpp Direct3D 9 files are actually BGR, so need to swizzle as well
                    uint32_t t1 = uint32_t(*(sPtr) << 16);
                    uint32_t t2 = uint32_t(*(sPtr + 1) << 8);
                    uint32_t t3 = uint32_t(*(sPtr + 2));

                    *(dPtr++) = t1 | t2 | t3 | 0xff000000;
                    sPtr += 3;
                }
                return true;
            }
            return false;

        case TEXP_LEGACY_R3G3B2:
            switch (outFormat)
            {
                case VK_FORMAT_R8G8B8A8_UNORM:
                    // D3DFMT_R3G3B2 -> DXGI_FORMAT_R8G8B8A8_UNORM
                    if (inSize >= 1 && outSize >= 4)
                    {
                        const uint8_t* __restrict sPtr = static_cast<const uint8_t*>(pSource);
                        uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);

                        for (size_t ocount = 0, icount = 0; ((icount < inSize) && (ocount < (outSize - 3))); ++icount, ocount += 4)
                        {
                            const uint8_t t = *(sPtr++);

                            uint32_t t1 = uint32_t((t & 0xe0) | ((t & 0xe0) >> 3) | ((t & 0xc0) >> 6));
                            uint32_t t2 = uint32_t(((t & 0x1c) << 11) | ((t & 0x1c) << 8) | ((t & 0x18) << 5));
                            uint32_t t3 = uint32_t(((t & 0x03) << 22) | ((t & 0x03) << 20) | ((t & 0x03) << 18) | ((t & 0x03) << 16));

                            *(dPtr++) = t1 | t2 | t3 | 0xff000000;
                        }
                        return true;
                    }
                    return false;

                case VK_FORMAT_B5G6R5_UNORM_PACK16:
                    // D3DFMT_R3G3B2 -> VK_FORMAT_B5G6R5_UNORM_PACK16
                    if (inSize >= 1 && outSize >= 2)
                    {
                        const uint8_t* __restrict sPtr = static_cast<const uint8_t*>(pSource);
                        uint16_t * __restrict dPtr = static_cast<uint16_t*>(pDestination);

                        for (size_t ocount = 0, icount = 0; ((icount < inSize) && (ocount < (outSize - 1))); ++icount, ocount += 2)
                        {
                            const unsigned t = *(sPtr++);

                            unsigned t1 = ((t & 0xe0u) << 8) | ((t & 0xc0u) << 5);
                            unsigned t2 = ((t & 0x1cu) << 6) | ((t & 0x1cu) << 3);
                            unsigned t3 = ((t & 0x03u) << 3) | ((t & 0x03u) << 1) | ((t & 0x02) >> 1);

                            *(dPtr++) = static_cast<uint16_t>(t1 | t2 | t3);
                        }
                        return true;
                    }
                    return false;

                default:
                    return false;
            }

        case TEXP_LEGACY_A8R3G3B2:
            if (outFormat != VK_FORMAT_R8G8B8A8_UNORM)
                return false;

            // D3DFMT_A8R3G3B2 -> DXGI_FORMAT_R8G8B8A8_UNORM
            if (inSize >= 2 && outSize >= 4)
            {
                const uint16_t* __restrict sPtr = static_cast<const uint16_t*>(pSource);
                uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);

                for (size_t ocount = 0, icount = 0; ((icount < (inSize - 1)) && (ocount < (outSize - 3))); icount += 2, ocount += 4)
                {
                    const uint16_t t = *(sPtr++);

                    uint32_t t1 = uint32_t((t & 0x00e0) | ((t & 0x00e0) >> 3) | ((t & 0x00c0) >> 6));
                    uint32_t t2 = uint32_t(((t & 0x001c) << 11) | ((t & 0x001c) << 8) | ((t & 0x0018) << 5));
                    uint32_t t3 = uint32_t(((t & 0x0003) << 22) | ((t & 0x0003) << 20) | ((t & 0x0003) << 18) | ((t & 0x0003) << 16));
                    uint32_t ta = (tflags & TEXP_SCANLINE_SETALPHA) ? 0xff000000 : uint32_t((t & 0xff00) << 16);

                    *(dPtr++) = t1 | t2 | t3 | ta;
                }
                return true;
            }
            return false;

        case TEXP_LEGACY_P8:
            if ((outFormat != VK_FORMAT_R8G8B8A8_UNORM) || !pal8)
                return false;

            // D3DFMT_P8 -> DXGI_FORMAT_R8G8B8A8_UNORM
            if (inSize >= 1 && outSize >= 4)
            {
                const uint8_t* __restrict sPtr = static_cast<const uint8_t*>(pSource);
                uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);

                for (size_t ocount = 0, icount = 0; ((icount < inSize) && (ocount < (outSize - 3))); ++icount, ocount += 4)
                {
                    uint8_t t = *(sPtr++);

                    *(dPtr++) = pal8[t];
                }
                return true;
            }
            return false;

        case TEXP_LEGACY_A8P8:
            if ((outFormat != VK_FORMAT_R8G8B8A8_UNORM) || !pal8)
                return false;

            // D3DFMT_A8P8 -> DXGI_FORMAT_R8G8B8A8_UNORM
            if (inSize >= 2 && outSize >= 4)
            {
                const uint16_t* __restrict sPtr = static_cast<const uint16_t*>(pSource);
                uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);

                for (size_t ocount = 0, icount = 0; ((icount < (inSize - 1)) && (ocount < (outSize - 3))); icount += 2, ocount += 4)
                {
                    const uint16_t t = *(sPtr++);

                    uint32_t t1 = pal8[t & 0xff];
                    uint32_t ta = (tflags & TEXP_SCANLINE_SETALPHA) ? 0xff000000 : uint32_t((t & 0xff00) << 16);

                    *(dPtr++) = t1 | ta;
                }
                return true;
            }
            return false;

        case TEXP_LEGACY_A4L4:
            switch (outFormat)
            {
            case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
                // D3DFMT_A4L4 -> VK_FORMAT_B4G4R4A4_UNORM_PACK16
                if (inSize >= 1 && outSize >= 2)
                {
                    const uint8_t * __restrict sPtr = static_cast<const uint8_t*>(pSource);
                    uint16_t * __restrict dPtr = static_cast<uint16_t*>(pDestination);

                    for (size_t ocount = 0, icount = 0; ((icount < inSize) && (ocount < (outSize - 1))); ++icount, ocount += 2)
                    {
                        const unsigned t = *(sPtr++);

                        unsigned t1 = (t & 0x0fu);
                        unsigned ta = (tflags & TEXP_SCANLINE_SETALPHA) ? 0xf000u : ((t & 0xf0u) << 8);

                        *(dPtr++) = static_cast<uint16_t>(t1 | (t1 << 4) | (t1 << 8) | ta);
                    }
                    return true;
                }
                return false;

            case VK_FORMAT_R8G8B8A8_UNORM:
                // D3DFMT_A4L4 -> VK_FORMAT_R8G8B8A8_UNORM
                if (inSize >= 1 && outSize >= 4)
                {
                    const uint8_t * __restrict sPtr = static_cast<const uint8_t*>(pSource);
                    uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);

                    for (size_t ocount = 0, icount = 0; ((icount < inSize) && (ocount < (outSize - 3))); ++icount, ocount += 4)
                    {
                        const uint8_t t = *(sPtr++);

                        uint32_t t1 = uint32_t(((t & 0x0f) << 4) | (t & 0x0f));
                        uint32_t ta = (tflags & TEXP_SCANLINE_SETALPHA) ? 0xff000000 : uint32_t(((t & 0xf0) << 24) | ((t & 0xf0) << 20));

                        *(dPtr++) = t1 | (t1 << 8) | (t1 << 16) | ta;
                    }
                    return true;
                }
                return false;

            default:
                return false;
            }

        case TEXP_LEGACY_B4G4R4A4:
            if (outFormat != VK_FORMAT_R8G8B8A8_UNORM)
                return false;

            // D3DFMT_A4R4G4B4 -> VK_FORMAT_R8G8B8A8_UNORM
            if (inSize >= 2 && outSize >= 4)
            {
                const uint16_t * __restrict sPtr = static_cast<const uint16_t*>(pSource);
                uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);

                for (size_t ocount = 0, icount = 0; ((icount < (inSize - 1)) && (ocount < (outSize - 3))); icount += 2, ocount += 4)
                {
                    const uint32_t t = *(sPtr++);

                    uint32_t t1 = uint32_t((t & 0x0f00) >> 4) | ((t & 0x0f00) >> 8);
                    uint32_t t2 = uint32_t((t & 0x00f0) << 8) | ((t & 0x00f0) << 4);
                    uint32_t t3 = uint32_t((t & 0x000f) << 20) | ((t & 0x000f) << 16);
                    uint32_t ta = uint32_t((tflags & TEXP_SCANLINE_SETALPHA) ? 0xff000000 : (((t & 0xf000) << 16) | ((t & 0xf000) << 12)));

                    *(dPtr++) = t1 | t2 | t3 | ta;
                }
                return true;
            }
            return false;

        case TEXP_LEGACY_L8:
            if (outFormat != VK_FORMAT_R8G8B8A8_UNORM)
                return false;

            // D3DFMT_L8 -> VK_FORMAT_R8G8B8A8_UNORM
            if (inSize >= 1 && outSize >= 4)
            {
                const uint8_t * __restrict sPtr = static_cast<const uint8_t*>(pSource);
                uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);

                for (size_t ocount = 0, icount = 0; ((icount < inSize) && (ocount < (outSize - 3))); ++icount, ocount += 4)
                {
                    uint32_t t1 = *(sPtr++);
                    uint32_t t2 = (t1 << 8);
                    uint32_t t3 = (t1 << 16);

                    *(dPtr++) = t1 | t2 | t3 | 0xff000000;
                }
                return true;
            }
            return false;

        case TEXP_LEGACY_L16:
            if (outFormat != VK_FORMAT_R16G16B16A16_UNORM)
                return false;

            // D3DFMT_L16 -> VK_FORMAT_R16G16B16A16_UNORM
            if (inSize >= 2 && outSize >= 8)
            {
                const uint16_t* __restrict sPtr = static_cast<const uint16_t*>(pSource);
                uint64_t * __restrict dPtr = static_cast<uint64_t*>(pDestination);

                for (size_t ocount = 0, icount = 0; ((icount < (inSize - 1)) && (ocount < (outSize - 7))); icount += 2, ocount += 8)
                {
                    const uint16_t t = *(sPtr++);

                    uint64_t t1 = t;
                    uint64_t t2 = (t1 << 16);
                    uint64_t t3 = (t1 << 32);

                    *(dPtr++) = t1 | t2 | t3 | 0xffff000000000000;
                }
                return true;
            }
            return false;

        case TEXP_LEGACY_A8L8:
            if (outFormat != VK_FORMAT_R8G8B8A8_UNORM)
                return false;

            // D3DFMT_A8L8 -> VK_FORMAT_R8G8B8A8_UNORM
            if (inSize >= 2 && outSize >= 4)
            {
                const uint16_t* __restrict sPtr = static_cast<const uint16_t*>(pSource);
                uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);

                for (size_t ocount = 0, icount = 0; ((icount < (inSize - 1)) && (ocount < (outSize - 3))); icount += 2, ocount += 4)
                {
                    const uint16_t t = *(sPtr++);

                    uint32_t t1 = uint32_t(t & 0xff);
                    uint32_t t2 = uint32_t(t1 << 8);
                    uint32_t t3 = uint32_t(t1 << 16);
                    uint32_t ta = (tflags & TEXP_SCANLINE_SETALPHA) ? 0xff000000 : uint32_t((t & 0xff00) << 16);

                    *(dPtr++) = t1 | t2 | t3 | ta;
                }
                return true;
            }
            return false;

        case TEXP_LEGACY_L6V5U5:
            if (outFormat != VK_FORMAT_R8G8B8A8_UNORM)
                return false;

            // D3DFMT_L6V5U5 -> VK_FORMAT_R8G8B8A8_UNORM (LUVA)
            if (inSize >= 2 && outSize >= 4)
            {
                const uint16_t* __restrict sPtr = static_cast<const uint16_t*>(pSource);
                uint32_t * __restrict dPtr = static_cast<uint32_t*>(pDestination);

                for (size_t ocount = 0, icount = 0; ((icount < (inSize - 1)) && (ocount < (outSize - 3))); icount += 2, ocount += 4)
                {
                    const uint16_t t = *(sPtr++);

                    // Converts unsigned 6-bit/signed 5-bit/signed 5-bit bump luminance to 8:8:8:8 unsigned
                    uint32_t t1 = ((t & 0xFC00) >> 8) | ((t & 0xC000) >> 14);

                    constexpr int m = 1U << 4;
                    int8_t v = ((((t >> 5) & 0x1f) ^ m) - m) + 16;
                    int8_t u = (((t & 0x1f) ^ m) - m) + 16;

                    auto t2 = static_cast<uint32_t>(u << 3 | u >> 2);
                    auto t3 = static_cast<uint32_t>(v << 3 | v >> 2);

                    *(dPtr++) = t1 | (t2 << 8) | (t3 << 16) | 0xff000000;
                }
                return true;
            }
            return false;

        default:
            return false;
        }
    }

    bool LegacyConvertScanline(
        void* pDestination,
        size_t outSize,
        VkFormat outFormat,
        const void* pSource,
        size_t inSize,
        TEXP_LEGACY_FORMAT inFormat,
        uint32_t tflags) noexcept
    {
        assert(pDestination && outSize > 0);
        assert(pSource && inSize > 0);

        switch (inFormat)
        {
        case TEXP_LEGACY_X8L8V8U8:
            if (outFormat != VK_FORMAT_R8G8B8A8_UNORM)
                return false;

            // D3DFMT_X8L8V8U8 -> DXGI_FORMAT_R8G8B8A8_UNORM (LUVA)
            if (inSize >= 4 && outSize >= 4)
            {
                auto sPtr = static_cast<const uint32_t*>(pSource);
                auto dPtr = static_cast<uint32_t*>(pDestination);

                for (size_t ocount = 0, icount = 0; ((icount < (inSize - 3)) && (ocount < (outSize - 3))); icount += 4, ocount += 4)
                {
                    const uint32_t t = *(sPtr++);

                    // Converts 8-bit unsigned / 8-bit signed / 8-bit signed to 8:8:8:8 unsigned
                    uint32_t t1 = (t >> 16) & 0xff;
                    constexpr int m = 1U << 7;
                    uint32_t v = ((((t >> 8) & 0xff) ^ m) - m) + 128;
                    uint32_t u = (((t & 0xff) ^ m) - m) + 128;

                    uint32_t t2 = u << 8;
                    uint32_t t3 = v << 16;

                    *(dPtr++) = t1 | t2 | t3 | 0xff000000;
                }
                return true;
            }
            return false;

        case TEXP_LEGACY_A2W10V10U10:
            if (outFormat != VK_FORMAT_A2R10G10B10_UNORM_PACK32)
                return false;

            // D3DFMT_A2W10V10U10 -> VK_FORMAT_A2R10G10B10_UNORM_PACK32 (UVWA) R10G10B10A2
            if (inSize >= 4 && outSize >= 4)
            {
                auto sPtr = static_cast<const uint32_t*>(pSource);
                auto dPtr = static_cast<uint32_t*>(pDestination);

                for (size_t ocount = 0, icount = 0; ((icount < (inSize - 3)) && (ocount < (outSize - 3))); icount += 4, ocount += 4)
                {
                    const uint32_t t = *(sPtr++);

                    // Converts 2-bit unsigned / 10-bit signed / 10-bit signed / 10-bit signed to 2:10:10:10 unsigned
                    constexpr int m = 1U << 9;
                    uint32_t w = ((((t >> 20) & 0x3ff) ^ m) - m) + 512;
                    uint32_t v = ((((t >> 10) & 0x3ff) ^ m) - m) + 512;
                    uint32_t u = (((t & 0x3ff) ^ m) - m) + 512;

                    uint32_t t1 = u;
                    uint32_t t2 = v << 10;
                    uint32_t t3 = w << 20;
                    uint32_t ta = (tflags & TEXP_SCANLINE_SETALPHA) ? 0xC0000000 : (t & 0xC0000000);

                    *(dPtr++) = t1 | t2 | t3 | ta;
                }
                return true;
            }
            return false;

        default:
            return false;
        }
    }

    //-------------------------------------------------------------------------------------
    // Converts or copies image data from pPixels into scratch image data
    //-------------------------------------------------------------------------------------
    bool CopyImage(
        const void* pPixels,
        size_t size,
        const TexMetadata& metadata,
        CP_FLAGS cpFlags,
        uint32_t convFlags,
        const uint32_t *pal8,
        const ScratchImage& image) noexcept
    {
        assert(pPixels);
        assert(image.GetPixels());

        if (!size)
            return false;

        if (convFlags & CONV_FLAGS_EXPAND)
        {
            if (convFlags & CONV_FLAGS_888)
                cpFlags |= CP_FLAGS_24BPP;
            else if (convFlags & (CONV_FLAGS_565 | CONV_FLAGS_5551 | CONV_FLAGS_4444 | CONV_FLAGS_8332 | CONV_FLAGS_A8P8 | CONV_FLAGS_L16 | CONV_FLAGS_A8L8 | CONV_FLAGS_L6V5U5))
                cpFlags |= CP_FLAGS_16BPP;
            else if (convFlags & (CONV_FLAGS_44 | CONV_FLAGS_332 | CONV_FLAGS_PAL8 | CONV_FLAGS_L8))
                cpFlags |= CP_FLAGS_8BPP;
        }

        size_t pixelSize, nimages;
        bool hr = DetermineImageArray(metadata, cpFlags, nimages, pixelSize);
        if (hr == false)
            return hr;

        if ((nimages == 0) || (nimages != image.GetImageCount()))
        {
            return false;
        }

        if (pixelSize > size)
        {
            return false;
        }

        std::unique_ptr<Image[]> timages(new (std::nothrow) Image[nimages]);
        if (!timages)
        {
            return false;
        }

        if (!SetupImageArray(
            const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(pPixels)),
            pixelSize,
            metadata,
            cpFlags,
            timages.get(),
            nimages))
        {
            return false;
        }

        if (nimages != image.GetImageCount())
        {
            return false;
        }

        const Image* images = image.GetImages();
        if (!images)
        {
            return false;
        }

        uint32_t tflags = (convFlags & CONV_FLAGS_NOALPHA) ? TEXP_SCANLINE_SETALPHA : 0u;
        if (convFlags & CONV_FLAGS_SWIZZLE)
            tflags |= TEXP_SCANLINE_LEGACY;

        switch (metadata.dimension)
        {
        case TEX_DIMENSION_TEXTURE1D:
        case TEX_DIMENSION_TEXTURE2D:
            {
                size_t index = 0;
                for (size_t item = 0; item < metadata.arraySize; ++item)
                {
                    size_t lastgood = 0;
                    for (size_t level = 0; level < metadata.mipLevels; ++level, ++index)
                    {
                        if (index >= nimages)
                            return false;

                        if (images[index].height != timages[index].height)
                            return false;

                        size_t dpitch = images[index].rowPitch;
                        const size_t spitch = timages[index].rowPitch;

                        const uint8_t *pSrc = timages[index].pixels;
                        if (!pSrc)
                            return false;

                        uint8_t *pDest = images[index].pixels;
                        if (!pDest)
                            return false;

                        if (IsCompressed(metadata.format))
                        {
                            size_t csize = std::min<size_t>(images[index].slicePitch, timages[index].slicePitch);
                            memcpy(pDest, pSrc, csize);

                            if (cpFlags & CP_FLAGS_BAD_DXTN_TAILS)
                            {
                                if (images[index].width < 4 || images[index].height < 4)
                                {
                                    csize = std::min<size_t>(images[index].slicePitch, timages[lastgood].slicePitch);
                                    memcpy(pDest, timages[lastgood].pixels, csize);
                                }
                                else
                                {
                                    lastgood = index;
                                }
                            }
                        }
                        else if (IsPlanar(metadata.format))
                        {
                            const size_t count = ComputeScanlines(metadata.format, images[index].height);
                            if (!count)
                                return false;

                            const size_t csize = std::min<size_t>(dpitch, spitch);
                            for (size_t h = 0; h < count; ++h)
                            {
                                memcpy(pDest, pSrc, csize);
                                pSrc += spitch;
                                pDest += dpitch;
                            }
                        }
                        else
                        {
                            for (size_t h = 0; h < images[index].height; ++h)
                            {
                                if (convFlags & CONV_FLAGS_EXPAND)
                                {
                                    if (convFlags & CONV_FLAGS_4444)
                                    {
                                        if (!ExpandScanline(pDest, dpitch, VK_FORMAT_R8G8B8A8_UNORM,
                                            pSrc, spitch,
                                            (convFlags & CONF_FLAGS_11ON12) ? VK_FORMAT_A4B4G4R4_UNORM_PACK16 : VK_FORMAT_B4G4R4A4_UNORM_PACK16,
                                            tflags))
                                            return false;
                                    }
                                    else if (convFlags & (CONV_FLAGS_565 | CONV_FLAGS_5551))
                                    {
                                        if (!ExpandScanline(pDest, dpitch, VK_FORMAT_R8G8B8A8_UNORM,
                                            pSrc, spitch,
                                            (convFlags & CONV_FLAGS_565) ? VK_FORMAT_B5G6R5_UNORM_PACK16 : VK_FORMAT_B5G5R5A1_UNORM_PACK16,
                                            tflags))
                                            return false;
                                    }
                                    else
                                    {
                                        const TEXP_LEGACY_FORMAT lformat = FindLegacyFormat(convFlags);
                                        if (!LegacyExpandScanline(pDest, dpitch, metadata.format,
                                            pSrc, spitch, lformat, pal8,
                                            tflags))
                                            return false;
                                    }
                                }
                                else if (convFlags & CONV_FLAGS_SWIZZLE)
                                {
                                    SwizzleScanline(pDest, dpitch, pSrc, spitch, metadata.format, tflags);
                                }
                                else if (convFlags & (CONV_FLAGS_L8U8V8 | CONV_FLAGS_WUV10))
                                {
                                    const TEXP_LEGACY_FORMAT lformat = FindLegacyFormat(convFlags);
                                    if (!LegacyConvertScanline(pDest, dpitch, metadata.format,
                                        pSrc, spitch, lformat, tflags))
                                        return false;
                                }
                                else
                                {
                                    CopyScanline(pDest, dpitch, pSrc, spitch, metadata.format, tflags);
                                }

                                pSrc += spitch;
                                pDest += dpitch;
                            }
                        }
                    }
                }
            }
            break;

        case TEX_DIMENSION_TEXTURE3D:
            {
                size_t index = 0;
                size_t d = metadata.depth;

                size_t lastgood = 0;
                for (size_t level = 0; level < metadata.mipLevels; ++level)
                {
                    for (size_t slice = 0; slice < d; ++slice, ++index)
                    {
                        if (index >= nimages)
                            return false;

                        if (images[index].height != timages[index].height)
                            return false;

                        size_t dpitch = images[index].rowPitch;
                        const size_t spitch = timages[index].rowPitch;

                        const uint8_t *pSrc = timages[index].pixels;
                        if (!pSrc)
                            return false;

                        uint8_t *pDest = images[index].pixels;
                        if (!pDest)
                            return false;

                        if (IsCompressed(metadata.format))
                        {
                            size_t csize = std::min<size_t>(images[index].slicePitch, timages[index].slicePitch);
                            memcpy(pDest, pSrc, csize);

                            if (cpFlags & CP_FLAGS_BAD_DXTN_TAILS)
                            {
                                if (images[index].width < 4 || images[index].height < 4)
                                {
                                    csize = std::min<size_t>(images[index].slicePitch, timages[lastgood + slice].slicePitch);
                                    memcpy(pDest, timages[lastgood + slice].pixels, csize);
                                }
                                else if (!slice)
                                {
                                    lastgood = index;
                                }
                            }
                        }
                        else if (IsPlanar(metadata.format))
                        {
                            // Direct3D does not support any planar formats for Texture3D
                            return false;
                        }
                        else
                        {
                            for (size_t h = 0; h < images[index].height; ++h)
                            {
                                if (convFlags & CONV_FLAGS_EXPAND)
                                {
                                    if (convFlags & CONV_FLAGS_4444)
                                    {
                                        if (!ExpandScanline(pDest, dpitch, VK_FORMAT_R8G8B8A8_UNORM,
                                            pSrc, spitch,
                                            (convFlags & CONF_FLAGS_11ON12) ? VK_FORMAT_A4B4G4R4_UNORM_PACK16 : VK_FORMAT_B4G4R4A4_UNORM_PACK16,
                                            tflags))
                                            return false;
                                    }
                                    else if (convFlags & (CONV_FLAGS_565 | CONV_FLAGS_5551))
                                    {
                                        if (!ExpandScanline(pDest, dpitch, VK_FORMAT_R8G8B8A8_UNORM,
                                            pSrc, spitch,
                                            (convFlags & CONV_FLAGS_565) ? VK_FORMAT_B5G6R5_UNORM_PACK16 : VK_FORMAT_B5G5R5A1_UNORM_PACK16,
                                            tflags))
                                            return false;
                                    }
                                    else
                                    {
                                        const TEXP_LEGACY_FORMAT lformat = FindLegacyFormat(convFlags);
                                        if (!LegacyExpandScanline(pDest, dpitch, metadata.format,
                                            pSrc, spitch, lformat, pal8,
                                            tflags))
                                            return false;
                                    }
                                }
                                else if (convFlags & CONV_FLAGS_SWIZZLE)
                                {
                                    SwizzleScanline(pDest, dpitch, pSrc, spitch, metadata.format, tflags);
                                }
                                else if (convFlags & (CONV_FLAGS_L8U8V8 | CONV_FLAGS_WUV10))
                                {
                                    const TEXP_LEGACY_FORMAT lformat = FindLegacyFormat(convFlags);
                                    if (!LegacyConvertScanline(pDest, dpitch, metadata.format,
                                        pSrc, spitch, lformat, tflags))
                                        return false;
                                }
                                else
                                {
                                    CopyScanline(pDest, dpitch, pSrc, spitch, metadata.format, tflags);
                                }

                                pSrc += spitch;
                                pDest += dpitch;
                            }
                        }
                    }

                    if (d > 1)
                        d >>= 1;
                }
            }
            break;

        default:
            return false;
        }

        return true;
    }

    bool CopyImageInPlace(uint32_t convFlags, const ScratchImage& image) noexcept
    {
        if (!image.GetPixels())
            return false;

        const Image* images = image.GetImages();
        if (!images)
            return false;

        const TexMetadata& metadata = image.GetMetadata();

        if (IsPlanar(metadata.format))
            return false;

        uint32_t tflags = (convFlags & CONV_FLAGS_NOALPHA) ? TEXP_SCANLINE_SETALPHA : 0u;
        if (convFlags & CONV_FLAGS_SWIZZLE)
            tflags |= TEXP_SCANLINE_LEGACY;

        for (size_t i = 0; i < image.GetImageCount(); ++i)
        {
            const Image* img = &images[i];
            uint8_t *pPixels = img->pixels;
            if (!pPixels)
                return false;

            size_t rowPitch = img->rowPitch;

            for (size_t h = 0; h < img->height; ++h)
            {
                if (convFlags & CONV_FLAGS_SWIZZLE)
                {
                    SwizzleScanline(pPixels, rowPitch, pPixels, rowPitch, metadata.format, tflags);
                }
                else if (convFlags & (CONV_FLAGS_L8U8V8 | CONV_FLAGS_WUV10))
                {
                    const TEXP_LEGACY_FORMAT lformat = FindLegacyFormat(convFlags);
                    if (!LegacyConvertScanline(pPixels, rowPitch, metadata.format, pPixels, rowPitch, lformat, tflags))
                    {
                        return false;
                    }
                }
                else
                {
                    CopyScanline(pPixels, rowPitch, pPixels, rowPitch, metadata.format, tflags);
                }

                pPixels += rowPitch;
            }
        }

        return true;
    }
}


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Obtain metadata from DDS file in memory/on disk
//-------------------------------------------------------------------------------------
bool VulkanTex::GetMetadataFromDDSMemory(
    const uint8_t* pSource,
    size_t size,
    DDS_FLAGS flags,
    TexMetadata& metadata) noexcept
{
    return GetMetadataFromDDSMemoryEx(pSource, size, flags, metadata, nullptr);
}


bool VulkanTex::GetMetadataFromDDSMemoryEx(
    const uint8_t* pSource,
    size_t size,
    DDS_FLAGS flags,
    TexMetadata& metadata,
    DDSMetaData* ddPixelFormat) noexcept
{
    if (!pSource || size == 0)
        return false;

    uint32_t convFlags = 0;
    return DecodeDDSHeader(pSource, size, flags, metadata, ddPixelFormat, convFlags);
}


bool VulkanTex::GetMetadataFromDDSFile(
    const wchar_t* szFile,
    DDS_FLAGS flags,
    TexMetadata& metadata) noexcept
{
    return GetMetadataFromDDSFileEx(szFile, flags, metadata, nullptr);
}


bool VulkanTex::GetMetadataFromDDSFileEx(
    const wchar_t* szFile,
    DDS_FLAGS flags,
    TexMetadata& metadata,
    DDSMetaData* ddPixelFormat) noexcept
{
    if (!szFile)
        return false;

    std::ifstream inFile(std::filesystem::path(szFile), std::ios::in | std::ios::binary | std::ios::ate);
    if (!inFile)
        return false;

    std::streampos fileLen = inFile.tellg();
    if (!inFile)
        return false;

    if (fileLen > UINT32_MAX)
        return false;

    inFile.seekg(0, std::ios::beg);
    if (!inFile)
        return false;

    const size_t len = fileLen;

    // Need at least enough data to fill the standard header and magic number to be a valid DDS
    if (len < DDS_MIN_HEADER_SIZE)
    {
        return false;
    }

    // Read the header in (including extended header if present)
    uint8_t header[DDS_DX10_HEADER_SIZE] = {};

    const auto headerLen = std::min<size_t>(len, DDS_DX10_HEADER_SIZE);

    inFile.read(reinterpret_cast<char*>(header), headerLen);
    if (!inFile)
        return false;

    uint32_t convFlags = 0;
    return DecodeDDSHeader(header, headerLen, flags, metadata, ddPixelFormat, convFlags);
}


//-------------------------------------------------------------------------------------
// Load a DDS file in memory
//-------------------------------------------------------------------------------------
bool VulkanTex::LoadFromDDSMemory(
    const uint8_t* pSource,
    size_t size,
    DDS_FLAGS flags,
    TexMetadata* metadata,
    ScratchImage& image) noexcept
{
    return LoadFromDDSMemoryEx(pSource, size, flags, metadata, nullptr, image);
}


bool VulkanTex::LoadFromDDSMemoryEx(
    const uint8_t* pSource,
    size_t size,
    DDS_FLAGS flags,
    TexMetadata* metadata,
    DDSMetaData* ddPixelFormat,
    ScratchImage& image) noexcept
{
    if (!pSource || size == 0)
        return false;

    image.Release();

    uint32_t convFlags = 0;
    TexMetadata mdata;
    bool hr = DecodeDDSHeader(pSource, size, flags, mdata, ddPixelFormat, convFlags);
    if (hr == false)
        return hr;

    size_t offset = DDS_MIN_HEADER_SIZE;
    if (convFlags & CONV_FLAGS_DX10)
        offset += sizeof(DDS_HEADER_DXT10);

    assert(offset <= size);

    const uint32_t *pal8 = nullptr;
    if (convFlags & CONV_FLAGS_PAL8)
    {
        pal8 = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(pSource) + offset);
        assert(pal8);
        offset += (256 * sizeof(uint32_t));
        if (size < offset)
            return false;
    }

    size_t remaining = size - offset;
    if (remaining == 0)
        return false;

    hr = image.Initialize(mdata);
    if (hr == false)
        return hr;

    if (flags & DDS_FLAGS_PERMISSIVE)
    {
        // For cubemaps, DDS_HEADER_DXT10.arraySize is supposed to be 'number of cubes'.
        // This handles cases where the value is incorrectly written as the original 6*numCubes value.
        if ((mdata.miscFlags & TEX_MISC_TEXTURECUBE)
            && (convFlags & CONV_FLAGS_DX10)
            && (image.GetPixelsSize() > remaining)
            && ((mdata.arraySize % 6) == 0))
        {
            mdata.arraySize = mdata.arraySize / 6;
            hr = image.Initialize(mdata);
            if (hr == false)
                return hr;

            if (image.GetPixelsSize() > remaining)
            {
                image.Release();
                return false;
            }
        }
    }

    CP_FLAGS cflags = CP_FLAGS_NONE;
    if (flags & DDS_FLAGS_LEGACY_DWORD)
    {
        cflags |= CP_FLAGS_LEGACY_DWORD;
    }
    if (flags & DDS_FLAGS_BAD_DXTN_TAILS)
    {
        cflags |= CP_FLAGS_BAD_DXTN_TAILS;
    }

    const void* pPixels = static_cast<const uint8_t*>(pSource) + offset;
    assert(pPixels);
    hr = CopyImage(pPixels,
        size - offset,
        mdata,
        cflags,
        convFlags,
        pal8,
        image);
    if (hr == false)
    {
        image.Release();
        return hr;
    }
    if (metadata)
        memcpy(metadata, &mdata, sizeof(TexMetadata));

    return true;
}


//-------------------------------------------------------------------------------------
// Load a DDS file from disk
//-------------------------------------------------------------------------------------
bool VulkanTex::LoadFromDDSFile(
    const wchar_t* szFile,
    DDS_FLAGS flags,
    TexMetadata* metadata,
    ScratchImage& image) noexcept
{
    return LoadFromDDSFileEx(szFile, flags, metadata, nullptr, image);
}

bool VulkanTex::LoadFromDDSFileEx(
    const wchar_t* szFile,
    DDS_FLAGS flags,
    TexMetadata* metadata,
    DDSMetaData* ddPixelFormat,
    ScratchImage& image) noexcept
{
    if (!szFile)
        return false;

    image.Release();

    std::ifstream inFile(std::filesystem::path(szFile), std::ios::in | std::ios::binary | std::ios::ate);
    if (!inFile)
        return false;

    std::streampos fileLen = inFile.tellg();
    if (!inFile)
        return false;

    if (fileLen > UINT32_MAX)
        return false;

    inFile.seekg(0, std::ios::beg);
    if (!inFile)
        return false;

    const size_t len = fileLen;

    // Need at least enough data to fill the standard header and magic number to be a valid DDS
    if (len < DDS_MIN_HEADER_SIZE)
    {
        return false;
    }

    // Read the header in (including extended header if present)
    uint8_t header[DDS_DX10_HEADER_SIZE] = {};

    const auto headerLen = std::min<size_t>(len, DDS_DX10_HEADER_SIZE);

    inFile.read(reinterpret_cast<char*>(header), headerLen);
    if (!inFile)
        return false;

    uint32_t convFlags = 0;
    TexMetadata mdata;
    bool hr = DecodeDDSHeader(header, headerLen, flags, mdata, ddPixelFormat, convFlags);
    if (hr == false)
        return hr;

    size_t offset = DDS_DX10_HEADER_SIZE;

    if (!(convFlags & CONV_FLAGS_DX10))
    {
        inFile.seekg(DDS_MIN_HEADER_SIZE, std::ios::beg);
        if (!inFile)
            return false;

        offset = DDS_MIN_HEADER_SIZE;
    }

    std::unique_ptr<uint32_t[]> pal8;
    if (convFlags & CONV_FLAGS_PAL8)
    {
        pal8.reset(new (std::nothrow) uint32_t[256]);
        if (!pal8)
        {
            return false;
        }

        inFile.read(reinterpret_cast<char*>(pal8.get()), 256 * sizeof(uint32_t));
        if (!inFile)
            return false;

        offset += (256 * sizeof(uint32_t));
    }

    const size_t remaining = len - offset;
    if (remaining == 0)
        return false;

    hr = image.Initialize(mdata);
    if (hr == false)
        return hr;

    if (flags & DDS_FLAGS_PERMISSIVE)
    {
        // For cubemaps, DDS_HEADER_DXT10.arraySize is supposed to be 'number of cubes'.
        // This handles cases where the value is incorrectly written as the original 6*numCubes value.
        if ((mdata.miscFlags & TEX_MISC_TEXTURECUBE)
            && (convFlags & CONV_FLAGS_DX10)
            && (image.GetPixelsSize() > remaining)
            && ((mdata.arraySize % 6) == 0))
        {
            mdata.arraySize = mdata.arraySize / 6;
            hr = image.Initialize(mdata);
            if (hr == false)
                return hr;

            if (image.GetPixelsSize() > remaining)
            {
                image.Release();
                return false;
            }
        }
    }

    if ((convFlags & CONV_FLAGS_EXPAND) || (flags & (DDS_FLAGS_LEGACY_DWORD | DDS_FLAGS_BAD_DXTN_TAILS)))
    {
        std::unique_ptr<uint8_t[]> temp(new (std::nothrow) uint8_t[remaining]);
        if (!temp)
        {
            image.Release();
            return false;
        }

        inFile.read(reinterpret_cast<char*>(temp.get()), remaining);
        if (!inFile)
        {
            image.Release();
            return false;
        }

        CP_FLAGS cflags = CP_FLAGS_NONE;
        if (flags & DDS_FLAGS_LEGACY_DWORD)
        {
            cflags |= CP_FLAGS_LEGACY_DWORD;
        }
        if (flags & DDS_FLAGS_BAD_DXTN_TAILS)
        {
            cflags |= CP_FLAGS_BAD_DXTN_TAILS;
        }

        hr = CopyImage(temp.get(),
            remaining,
            mdata,
            cflags,
            convFlags,
            pal8.get(),
            image);
        if (hr == false)
        {
            image.Release();
            return hr;
        }
    }
    else
    {
        if (remaining < image.GetPixelsSize())
        {
            image.Release();
            return false;
        }

        if (image.GetPixelsSize() > UINT32_MAX)
        {
            image.Release();
            return false;
        }

        inFile.read(reinterpret_cast<char*>(image.GetPixels()), image.GetPixelsSize());
        if (!inFile)
        {
            image.Release();
            return false;
        }

        if (convFlags & (CONV_FLAGS_SWIZZLE | CONV_FLAGS_NOALPHA | CONV_FLAGS_L8U8V8 | CONV_FLAGS_WUV10))
        {
            // Swizzle/copy image in place
            hr = CopyImageInPlace(convFlags, image);
            if (hr == false)
            {
                image.Release();
                return hr;
            }
        }
    }

    if (metadata)
        memcpy(metadata, &mdata, sizeof(TexMetadata));

    return true;
}


//-------------------------------------------------------------------------------------
// Save a DDS file to memory
//-------------------------------------------------------------------------------------
bool VulkanTex::SaveToDDSMemory(
    const Image* images,
    size_t nimages,
    const TexMetadata& metadata,
    DDS_FLAGS flags,
    Blob& blob) noexcept
{
    if (!images || (nimages == 0))
        return false;

    // Determine memory required
    size_t required = 0;
    bool hr = EncodeDDSHeader(metadata, flags, nullptr, 0, required);
    if (hr == false)
        return hr;

    bool fastpath = true;
    const bool use24bpp = ((metadata.format == VK_FORMAT_B8G8R8_UNORM)
        && (flags & DDS_FLAGS_FORCE_24BPP_RGB)
        && !(flags & (DDS_FLAGS_FORCE_DX10_EXT | DDS_FLAGS_FORCE_DX10_EXT_MISC2))) != 0;

    for (size_t i = 0; i < nimages; ++i)
    {
        if (!images[i].pixels)
            return false;

        if (images[i].format != metadata.format)
            return false;

        size_t ddsRowPitch, ddsSlicePitch;
        hr = ComputePitch(metadata.format,
                    images[i].width, images[i].height,
                    ddsRowPitch, ddsSlicePitch,
                    (use24bpp) ? CP_FLAGS_24BPP : CP_FLAGS_NONE);

        if (hr == false)
            return hr;

        assert(images[i].rowPitch > 0);
        assert(images[i].slicePitch > 0);

        if ((images[i].rowPitch != ddsRowPitch) || (images[i].slicePitch != ddsSlicePitch))
        {
            fastpath = false;
        }

        required += ddsSlicePitch;
    }

    assert(required > 0);

    blob.Release();

    hr = blob.Initialize(required);

    if (hr == false)
        return hr;

    auto pDestination = blob.GetBufferPointer();
    assert(pDestination);

    hr = EncodeDDSHeader(metadata, flags, pDestination, blob.GetBufferSize(), required);

    if (hr == false)
    {
        blob.Release();
        return hr;
    }

    size_t remaining = blob.GetBufferSize() - required;
    pDestination += required;

    if (remaining == 0)
    {
        blob.Release();
        return false;
    }

    switch (static_cast<DDS_RESOURCE_DIMENSION>(metadata.dimension))
    {
        case DDS_DIMENSION_TEXTURE1D:
        case DDS_DIMENSION_TEXTURE2D:
        {
            size_t index = 0;

            for (size_t item = 0; item < metadata.arraySize; ++item)
            {
                for (size_t level = 0; level < metadata.mipLevels; ++level)
                {
                    if (index >= nimages)
                    {
                        blob.Release();
                        return false;
                    }

                    if (fastpath)
                    {
                        size_t pixsize = images[index].slicePitch;
                        memcpy(pDestination, images[index].pixels, pixsize);

                        pDestination += pixsize;
                        remaining -= pixsize;
                    }
                    else if (use24bpp)
                    {
                        size_t ddsRowPitch, ddsSlicePitch;
                        hr = ComputePitch(metadata.format, images[index].width, images[index].height, ddsRowPitch, ddsSlicePitch, CP_FLAGS_24BPP);
                        if (hr == false)
                        {
                            blob.Release();
                            return hr;
                        }

                        const size_t rowPitch = images[index].rowPitch;
                        const uint8_t * __restrict sPtr = images[index].pixels;
                        uint8_t * __restrict dPtr = pDestination;

                        const size_t csize = std::min<size_t>(metadata.width * 3, ddsRowPitch);
                        size_t tremaining = remaining;
                        for (size_t j = 0; j < images[index].height; ++j)
                        {
                            if (tremaining < csize)
                            {
                                blob.Release();
                                return false;
                            }

                            CopyScanline24bpp(dPtr, sPtr, images[index].width);

                            sPtr += rowPitch;
                            dPtr += ddsRowPitch;
                            tremaining -= ddsRowPitch;
                        }

                        pDestination += ddsSlicePitch;
                        remaining -= ddsSlicePitch;
                    }
                    else
                    {
                        size_t ddsRowPitch, ddsSlicePitch;
                        hr = ComputePitch(metadata.format, images[index].width, images[index].height, ddsRowPitch, ddsSlicePitch, CP_FLAGS_NONE);
                        if (hr == false)
                        {
                            blob.Release();
                            return hr;
                        }

                        const size_t rowPitch = images[index].rowPitch;

                        const uint8_t * __restrict sPtr = images[index].pixels;
                        uint8_t * __restrict dPtr = pDestination;

                        const size_t lines = ComputeScanlines(metadata.format, images[index].height);
                        const size_t csize = std::min<size_t>(rowPitch, ddsRowPitch);
                        size_t tremaining = remaining;
                        for (size_t j = 0; j < lines; ++j)
                        {
                            if (tremaining < csize)
                            {
                                blob.Release();
                                return false;
                            }

                            memcpy(dPtr, sPtr, csize);

                            sPtr += rowPitch;
                            dPtr += ddsRowPitch;
                            tremaining -= ddsRowPitch;
                        }

                        pDestination += ddsSlicePitch;
                        remaining -= ddsSlicePitch;
                    }

                    ++index;
                }
            }

            break;
        }

        case DDS_DIMENSION_TEXTURE3D:
        {
            if (metadata.arraySize != 1)
            {
                blob.Release();
                return false;
            }

            size_t d = metadata.depth;

            size_t index = 0;

            for (size_t level = 0; level < metadata.mipLevels; ++level)
            {
                for (size_t slice = 0; slice < d; ++slice)
                {
                    if (index >= nimages)
                    {
                        blob.Release();
                        return false;
                    }

                    if (fastpath)
                    {
                        size_t pixsize = images[index].slicePitch;
                        memcpy(pDestination, images[index].pixels, pixsize);

                        pDestination += pixsize;
                        remaining -= pixsize;
                    }
                    else if (use24bpp)
                    {
                        size_t ddsRowPitch, ddsSlicePitch;
                        hr = ComputePitch(metadata.format, images[index].width, images[index].height, ddsRowPitch, ddsSlicePitch, CP_FLAGS_24BPP);
                        if (hr == false)
                        {
                            blob.Release();
                            return hr;
                        }

                        const size_t rowPitch = images[index].rowPitch;
                        const uint8_t * __restrict sPtr = images[index].pixels;
                        uint8_t * __restrict dPtr = pDestination;

                        const size_t csize = std::min<size_t>(metadata.width * 3, ddsRowPitch);
                        size_t tremaining = remaining;
                        for (size_t j = 0; j < images[index].height; ++j)
                        {
                            if (tremaining < csize)
                            {
                                blob.Release();
                                return false;
                            }

                            CopyScanline24bpp(dPtr, sPtr, images[index].width);

                            sPtr += rowPitch;
                            dPtr += ddsRowPitch;
                            tremaining -= ddsRowPitch;
                        }

                        pDestination += ddsSlicePitch;
                        remaining -= ddsSlicePitch;
                    }
                    else
                    {
                        size_t ddsRowPitch, ddsSlicePitch;
                        hr = ComputePitch(metadata.format, images[index].width, images[index].height, ddsRowPitch, ddsSlicePitch, CP_FLAGS_NONE);
                        if (hr == false)
                        {
                            blob.Release();
                            return hr;
                        }

                        const size_t rowPitch = images[index].rowPitch;

                        const uint8_t * __restrict sPtr = images[index].pixels;
                        uint8_t * __restrict dPtr = pDestination;

                        const size_t lines = ComputeScanlines(metadata.format, images[index].height);
                        const size_t csize = std::min<size_t>(rowPitch, ddsRowPitch);
                        size_t tremaining = remaining;

                        for (size_t j = 0; j < lines; ++j)
                        {
                            if (tremaining < csize)
                            {
                                blob.Release();
                                return false;
                            }

                            memcpy(dPtr, sPtr, csize);

                            sPtr += rowPitch;
                            dPtr += ddsRowPitch;
                            tremaining -= ddsRowPitch;
                        }

                        pDestination += ddsSlicePitch;
                        remaining -= ddsSlicePitch;
                    }

                    ++index;
                }

                if (d > 1)
                    d >>= 1;
            }

            break;
        }

        default:
            blob.Release();
            return false;
    }

    return true;
}

//-------------------------------------------------------------------------------------
// Save a DDS file to disk
//-------------------------------------------------------------------------------------
bool VulkanTex::SaveToDDSFile(
    const Image* images,
    size_t nimages,
    const TexMetadata& metadata,
    DDS_FLAGS flags,
    const char* szFile) noexcept
{
    if (szFile == nullptr)
        return false;

    // Create DDS Header
    uint8_t header[DDS_DX10_HEADER_SIZE] = {};
    size_t  required                     = 0;

    bool hr = EncodeDDSHeader(metadata, flags, header, DDS_DX10_HEADER_SIZE, required);

    if (hr == false)
        return hr;

    // Create file and write header
    std::ofstream outFile{ std::filesystem::path(szFile), std::ios::out | std::ios::binary | std::ios::trunc };

    if (!outFile)
        return false;

    outFile.write(reinterpret_cast<char*>(header), static_cast<std::streamsize>(required));

    if (!outFile)
        return false;

    const bool use24bpp = ((metadata.format == VK_FORMAT_B8G8R8_UNORM) &&
                           (flags & DDS_FLAGS_FORCE_24BPP_RGB) &&
                           !(flags & (DDS_FLAGS_FORCE_DX10_EXT | DDS_FLAGS_FORCE_DX10_EXT_MISC2))) != 0;

    std::unique_ptr<uint8_t[]> tempRow = nullptr;

    if (use24bpp)
    {
        uint64_t lineSize = uint64_t(metadata.width) * 3;

        if (lineSize > UINT32_MAX)
        {
            return false;
        }

        tempRow.reset(new (std::nothrow) uint8_t[static_cast<size_t>(lineSize)]);

        if (!tempRow)
        {
            return false;
        }
    }

    // Write images
    switch (static_cast<DDS_RESOURCE_DIMENSION>(metadata.dimension))
    {
        case DDS_DIMENSION_TEXTURE1D:
        case DDS_DIMENSION_TEXTURE2D:
        {
            size_t index = 0;

            for (size_t item = 0; item < metadata.arraySize; ++item)
            {
                for (size_t level = 0; level < metadata.mipLevels; ++level, ++index)
                {
                    if (index >= nimages)
                        return false;

                    if (!images[index].pixels)
                        return false;

                    assert(images[index].rowPitch > 0);
                    assert(images[index].slicePitch > 0);

                    size_t ddsRowPitch   = 0;
                    size_t ddsSlicePitch = 0;

                    hr = ComputePitch(metadata.format,
                                      images[index].width, images[index].height,
                                      ddsRowPitch, ddsSlicePitch,
                                      (use24bpp) ? CP_FLAGS_24BPP : CP_FLAGS_NONE);

                    if (hr == false)
                        return hr;

                    if ((images[index].slicePitch == ddsSlicePitch) && (ddsSlicePitch <= UINT32_MAX))
                    {
                        outFile.write(reinterpret_cast<char*>(images[index].pixels),
                                      static_cast<std::streamsize>(ddsSlicePitch));

                        if (!outFile)
                            return false;
                    }
                    else if (use24bpp)
                    {
                        const size_t               rowPitch = images[index].rowPitch;
                        const uint8_t * __restrict sPtr     = images[index].pixels;

                        assert(ddsRowPitch <= metadata.width * 3u);

                        for (size_t j = 0; j < images[index].height; ++j)
                        {
                            CopyScanline24bpp(tempRow.get(), sPtr, images[index].width);

                            outFile.write(reinterpret_cast<const char*>(tempRow.get()),
                                          static_cast<std::streamsize>(ddsRowPitch));

                            if (!outFile)
                                return false;

                            sPtr += rowPitch;
                        }
                    }
                    else
                    {
                        const size_t rowPitch = images[index].rowPitch;

                        if (rowPitch < ddsRowPitch)
                        {
                            // DDS uses 1-byte alignment, so if this is happening then the input pitch isn't actually a full line of data
                            return false;
                        }

                        if (ddsRowPitch > UINT32_MAX)
                            return false;

                        const uint8_t * __restrict sPtr = images[index].pixels;

                        const size_t lines = ComputeScanlines(metadata.format, images[index].height);

                        for (size_t j = 0; j < lines; ++j)
                        {
                            outFile.write(reinterpret_cast<const char*>(sPtr), static_cast<std::streamsize>(ddsRowPitch));

                            if (!outFile)
                                return false;

                            sPtr += rowPitch;
                        }
                    }
                }
            }

            break;
        }

        case DDS_DIMENSION_TEXTURE3D:
        {
            if (metadata.arraySize != 1)
                return false;

            size_t d     = metadata.depth;
            size_t index = 0;

            for (size_t level = 0; level < metadata.mipLevels; ++level)
            {
                for (size_t slice = 0; slice < d; ++slice, ++index)
                {
                    if (index >= nimages)
                        return false;

                    if (!images[index].pixels)
                        return false;

                    assert(images[index].rowPitch > 0);
                    assert(images[index].slicePitch > 0);

                    size_t ddsRowPitch   = 0;
                    size_t ddsSlicePitch = 0;

                    hr = ComputePitch(metadata.format,
                                images[index].width, images[index].height,
                                ddsRowPitch, ddsSlicePitch,
                                (use24bpp) ? CP_FLAGS_24BPP : CP_FLAGS_NONE);

                    if (hr == false)
                        return hr;

                    if ((images[index].slicePitch == ddsSlicePitch) && (ddsSlicePitch <= UINT32_MAX))
                    {
                        outFile.write(reinterpret_cast<char*>(images[index].pixels),
                                      static_cast<std::streamsize>(ddsSlicePitch));

                        if (!outFile)
                            return false;
                    }
                    else if (use24bpp)
                    {
                        const size_t rowPitch = images[index].rowPitch;
                        const uint8_t * __restrict sPtr = images[index].pixels;

                        assert(ddsRowPitch <= metadata.width * 3u);

                        for (size_t j = 0; j < images[index].height; ++j)
                        {
                            CopyScanline24bpp(tempRow.get(), sPtr, images[index].width);

                            outFile.write(reinterpret_cast<const char*>(tempRow.get()),
                                          static_cast<std::streamsize>(ddsRowPitch));

                            if (!outFile)
                                return false;

                            sPtr += rowPitch;
                        }
                    }
                    else
                    {
                        const size_t rowPitch = images[index].rowPitch;
                        if (rowPitch < ddsRowPitch)
                        {
                            // DDS uses 1-byte alignment, so if this is happening then the input pitch isn't actually a full line of data
                            return false;
                        }

                        if (ddsRowPitch > UINT32_MAX)
                            return false;

                        const uint8_t * __restrict sPtr = images[index].pixels;
                        const size_t lines = ComputeScanlines(metadata.format, images[index].height);

                        for (size_t j = 0; j < lines; ++j)
                        {
                            outFile.write(reinterpret_cast<const char*>(sPtr),
                                          static_cast<std::streamsize>(ddsRowPitch));

                            if (!outFile)
                                return false;

                            sPtr += rowPitch;
                        }
                    }
                }

                if (d > 1)
                    d >>= 1;
            }

            break;
        }

        default:
            return false;
    }

    return true;
}
