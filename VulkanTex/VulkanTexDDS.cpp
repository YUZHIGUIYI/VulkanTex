#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vulkan/vulkan_core.h>
#include "VulkanTex.h"
#include "VulkanTexDDS.h"

using namespace VulkanTex;

static_assert(static_cast<int>(TEX_DIMENSION_TEXTURE1D) == static_cast<int>(DDS_DIMENSION_TEXTURE1D), "header enum mismatch");
static_assert(static_cast<int>(TEX_DIMENSION_TEXTURE2D) == static_cast<int>(DDS_DIMENSION_TEXTURE2D), "header enum mismatch");
static_assert(static_cast<int>(TEX_DIMENSION_TEXTURE3D) == static_cast<int>(DDS_DIMENSION_TEXTURE3D), "header enum mismatch");

namespace DXGI
{
    constexpr uint32_t FORMAT_UNKNOWN             = 0;
    // 128-bit
    constexpr uint32_t FORMAT_R32G32B32A32_FLOAT  = 2;
    constexpr uint32_t FORMAT_R32G32B32A32_UINT   = 3;
    constexpr uint32_t FORMAT_R32G32B32A32_SINT   = 4;
    // 64-bit
    constexpr uint32_t FORMAT_R16G16B16A16_FLOAT  = 10;
    constexpr uint32_t FORMAT_R16G16B16A16_UNORM  = 11;
    constexpr uint32_t FORMAT_R16G16B16A16_UINT   = 12;
    constexpr uint32_t FORMAT_R16G16B16A16_SINT   = 13;
    constexpr uint32_t FORMAT_R32G32_FLOAT        = 16;
    constexpr uint32_t FORMAT_R32G32_UINT         = 17;
    constexpr uint32_t FORMAT_R32G32_SINT         = 18;
    // 32-bit
    constexpr uint32_t FORMAT_R10G10B10A2_UNORM   = 24;
    constexpr uint32_t FORMAT_R10G10B10A2_UINT    = 25;
    constexpr uint32_t FORMAT_R11G11B10_FLOAT     = 26;
    constexpr uint32_t FORMAT_R8G8B8A8_UNORM      = 28;
    constexpr uint32_t FORMAT_R8G8B8A8_UNORM_SRGB = 29;
    constexpr uint32_t FORMAT_R8G8B8A8_UINT       = 30;
    constexpr uint32_t FORMAT_R8G8B8A8_SINT       = 31;
    constexpr uint32_t FORMAT_R16G16_FLOAT        = 34;
    constexpr uint32_t FORMAT_R16G16_UNORM        = 35;
    constexpr uint32_t FORMAT_R16G16_UINT         = 36;
    constexpr uint32_t FORMAT_R16G16_SINT         = 37;
    constexpr uint32_t FORMAT_R32_FLOAT           = 41;
    constexpr uint32_t FORMAT_R32_UINT            = 42;
    constexpr uint32_t FORMAT_R32_SINT            = 43;
    // 32-bit (Swapped Channels)
    constexpr uint32_t FORMAT_B8G8R8A8_UNORM      = 87;
    constexpr uint32_t FORMAT_B8G8R8A8_UNORM_SRGB = 91;
    // 16-bit
    constexpr uint32_t FORMAT_R8G8_UNORM          = 49;
    constexpr uint32_t FORMAT_R8G8_UINT           = 50;
    constexpr uint32_t FORMAT_R8G8_SINT           = 51;
    constexpr uint32_t FORMAT_R16_FLOAT           = 54;
    constexpr uint32_t FORMAT_R16_UNORM           = 56;
    constexpr uint32_t FORMAT_R16_UINT            = 57;
    constexpr uint32_t FORMAT_R16_SINT            = 58;
    // 8-bit
    constexpr uint32_t FORMAT_R8_UNORM            = 61;
    constexpr uint32_t FORMAT_R8_UINT             = 62;
    constexpr uint32_t FORMAT_R8_SINT             = 63;
}

namespace VulkanTex
{
    uint32_t VkFormatToDXGIFormat(VkFormat vkFormat)
    {
        switch (vkFormat)
        {
            // 8-bit RGBA (32 bits total)
            case VK_FORMAT_R8G8B8A8_UNORM: return DXGI::FORMAT_R8G8B8A8_UNORM;
            case VK_FORMAT_R8G8B8A8_SRGB:  return DXGI::FORMAT_R8G8B8A8_UNORM_SRGB;
            case VK_FORMAT_R8G8B8A8_UINT:  return DXGI::FORMAT_R8G8B8A8_UINT;
            case VK_FORMAT_R8G8B8A8_SINT:  return DXGI::FORMAT_R8G8B8A8_SINT;
            // 8-bit BGRA (32 bits total) 
            case VK_FORMAT_B8G8R8A8_UNORM: return DXGI::FORMAT_B8G8R8A8_UNORM;
            case VK_FORMAT_B8G8R8A8_SRGB:  return DXGI::FORMAT_B8G8R8A8_UNORM_SRGB;
            // 10-bit & 11-bit Packed (32 bits total)
            // [Bit Layout]: A:30-31, B:20-29, G:10-19, R:0-9
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
                return DXGI::FORMAT_R10G10B10A2_UNORM;
            case VK_FORMAT_A2B10G10R10_UINT_PACK32:
                return DXGI::FORMAT_R10G10B10A2_UINT;
            // [Swizzle Case]: Need swizzle
            // Vulkan: A:30-31, R:20-29, G:10-19, B:0-9
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
                return DXGI::FORMAT_R10G10B10A2_UNORM;
            case VK_FORMAT_A2R10G10B10_UINT_PACK32:
                return DXGI::FORMAT_R10G10B10A2_UINT;
            // 11-11-10 Float
            // Bit Layout: B:22-31, G:11-21, R:0-10
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
                return DXGI::FORMAT_R11G11B10_FLOAT;
            // 16-bit RGBA (64 bits total)
            case VK_FORMAT_R16G16B16A16_SFLOAT: return DXGI::FORMAT_R16G16B16A16_FLOAT;
            case VK_FORMAT_R16G16B16A16_UNORM:  return DXGI::FORMAT_R16G16B16A16_UNORM;
            case VK_FORMAT_R16G16B16A16_UINT:   return DXGI::FORMAT_R16G16B16A16_UINT;
            case VK_FORMAT_R16G16B16A16_SINT:   return DXGI::FORMAT_R16G16B16A16_SINT;
            // 32-bit RGBA (128 bits total)
            case VK_FORMAT_R32G32B32A32_SFLOAT: return DXGI::FORMAT_R32G32B32A32_FLOAT;
            case VK_FORMAT_R32G32B32A32_UINT:   return DXGI::FORMAT_R32G32B32A32_UINT;
            case VK_FORMAT_R32G32B32A32_SINT:   return DXGI::FORMAT_R32G32B32A32_SINT;
            // Dual Channel (RG)
            case VK_FORMAT_R8G8_UNORM:    return DXGI::FORMAT_R8G8_UNORM;
            case VK_FORMAT_R8G8_UINT:     return DXGI::FORMAT_R8G8_UINT;
            case VK_FORMAT_R8G8_SINT:     return DXGI::FORMAT_R8G8_SINT;
            case VK_FORMAT_R16G16_SFLOAT: return DXGI::FORMAT_R16G16_FLOAT;
            case VK_FORMAT_R16G16_UNORM:  return DXGI::FORMAT_R16G16_UNORM;
            case VK_FORMAT_R16G16_UINT:   return DXGI::FORMAT_R16G16_UINT;
            case VK_FORMAT_R16G16_SINT:   return DXGI::FORMAT_R16G16_SINT;
            case VK_FORMAT_R32G32_SFLOAT: return DXGI::FORMAT_R32G32_FLOAT;
            case VK_FORMAT_R32G32_UINT:   return DXGI::FORMAT_R32G32_UINT;
            case VK_FORMAT_R32G32_SINT:   return DXGI::FORMAT_R32G32_SINT;
            // Single Channel (R)
            case VK_FORMAT_R8_UNORM:   return DXGI::FORMAT_R8_UNORM;
            case VK_FORMAT_R8_UINT:    return DXGI::FORMAT_R8_UINT;
            case VK_FORMAT_R8_SINT:    return DXGI::FORMAT_R8_SINT;
            case VK_FORMAT_R16_SFLOAT: return DXGI::FORMAT_R16_FLOAT;
            case VK_FORMAT_R16_UNORM:  return DXGI::FORMAT_R16_UNORM;
            case VK_FORMAT_R16_UINT:   return DXGI::FORMAT_R16_UINT;
            case VK_FORMAT_R16_SINT:   return DXGI::FORMAT_R16_SINT;
            case VK_FORMAT_R32_SFLOAT: return DXGI::FORMAT_R32_FLOAT;
            case VK_FORMAT_R32_UINT:   return DXGI::FORMAT_R32_UINT;
            case VK_FORMAT_R32_SINT:   return DXGI::FORMAT_R32_SINT;
            // Others
            default:
                return DXGI::FORMAT_UNKNOWN;
        }
    }
}

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

    void CopyScanline24bpp(
        uint8_t* pDestination,
        const uint8_t* pSource,
        size_t width) noexcept
    {
        for (size_t x = 0; x < width; ++x)
        {
            pDestination[0] = pSource[0]; // B
            pDestination[1] = pSource[1]; // G
            pDestination[2] = pSource[2]; // R

            pSource      += 4;
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
        ext->dxgiFormat        = VulkanTex::VkFormatToDXGIFormat(metadata.format);
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
