#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vulkan/vulkan.hpp>

#define VUlKAN_TEX_VERSION 209

namespace VulkanTex
{
    // Format type
    enum FORMAT_TYPE : uint32_t
    {
        FORMAT_TYPE_TYPELESS,
        FORMAT_TYPE_FLOAT,
        FORMAT_TYPE_UNORM,
        FORMAT_TYPE_SNORM,
        FORMAT_TYPE_UINT,
        FORMAT_TYPE_SINT,
    };

    //---------------------------------------------------------------------------------
    // Texture metadata
    enum TEX_DIMENSION : uint32_t
    {
        TEX_DIMENSION_TEXTURE1D = 2,
        TEX_DIMENSION_TEXTURE2D = 3,
        TEX_DIMENSION_TEXTURE3D = 4,
    };

    enum TEX_MISC_FLAG : uint32_t
    {
        TEX_MISC_TEXTURECUBE = 0x4L,
    };

    enum TEX_MISC_FLAG2 : uint32_t
    {
        TEX_MISC2_ALPHA_MODE_MASK = 0x7L,
    };

    enum TEX_ALPHA_MODE : uint32_t
    {
        TEX_ALPHA_MODE_UNKNOWN = 0,
        TEX_ALPHA_MODE_STRAIGHT = 1,
        TEX_ALPHA_MODE_PREMULTIPLIED = 2,
        TEX_ALPHA_MODE_OPAQUE = 3,
        TEX_ALPHA_MODE_CUSTOM = 4,
    };

    enum CP_FLAGS : uint32_t
    {
        // Normal operation
        CP_FLAGS_NONE = 0x0,

        // Assume pitch is DWORD aligned instead of BYTE aligned
        CP_FLAGS_LEGACY_DWORD = 0x1,

        // Assume pitch is 16-byte aligned instead of BYTE aligned
        CP_FLAGS_PARAGRAPH = 0x2,

        // Assume pitch is 32-byte aligned instead of BYTE aligned
        CP_FLAGS_YMM = 0x4,

        // Assume pitch is 64-byte aligned instead of BYTE aligned
        CP_FLAGS_ZMM = 0x8,

        // Assume pitch is 4096-byte aligned instead of BYTE aligned
        CP_FLAGS_PAGE4K = 0x200,

        // BC formats with malformed mipchain blocks smaller than 4x4
        CP_FLAGS_BAD_DXTN_TAILS = 0x1000,

        // Override with a legacy 24 bits-per-pixel format size
        CP_FLAGS_24BPP = 0x10000,

        // Override with a legacy 16 bits-per-pixel format size
        CP_FLAGS_16BPP = 0x20000,

        // Override with a legacy 8 bits-per-pixel format size
        CP_FLAGS_8BPP = 0x40000,

        // Don't allow pixel allocations in excess of 4GB (always true for 32-bit)
        CP_FLAGS_LIMIT_4GB = 0x10000000,
    };

    enum DDS_FLAGS : uint32_t
    {
        DDS_FLAGS_NONE = 0x0,

        // Assume pitch is DWORD aligned instead of BYTE aligned (used by some legacy DDS files)
        DDS_FLAGS_LEGACY_DWORD = 0x1,

        // Do not implicitly convert legacy formats that result in larger pixel sizes (24 bpp, 3:3:2, A8L8, A4L4, P8, A8P8)
        DDS_FLAGS_NO_LEGACY_EXPANSION = 0x2,

        // Do not use work-around for long-standing D3DX DDS file format issue which reversed the 10:10:10:2 color order masks
        DDS_FLAGS_NO_R10B10G10A2_FIXUP = 0x4,

        // Convert DXGI 1.1 BGR formats to VkFormat_R8G8B8A8_UNORM to avoid use of optional WDDM 1.1 formats
        DDS_FLAGS_FORCE_RGB = 0x8,

        // Conversions avoid use of 565, 5551, and 4444 formats and instead expand to 8888 to avoid use of optional WDDM 1.2 formats
        DDS_FLAGS_NO_16BPP = 0x10,

        // When loading legacy luminance formats expand replicating the color channels rather than leaving them packed (L8, L16, A8L8)
        DDS_FLAGS_EXPAND_LUMINANCE = 0x20,

        // Some older DXTn DDS files incorrectly handle mipchain tails for blocks smaller than 4x4
        DDS_FLAGS_BAD_DXTN_TAILS = 0x40,

        // Allow some file variants due to common bugs in the header written by various leagcy DDS writers
        DDS_FLAGS_PERMISSIVE = 0x80,

        // Allow some files to be read that have incorrect mipcount values in the header by only reading the top-level mip
        DDS_FLAGS_IGNORE_MIPS = 0x100,

        // Always use the 'DX10' header extension for DDS writer (i.e. don't try to write DX9 compatible DDS files)
        DDS_FLAGS_FORCE_DX10_EXT = 0x10000,

        // DDS_FLAGS_FORCE_DX10_EXT including miscFlags2 information (result may not be compatible with D3DX10 or D3DX11)
        DDS_FLAGS_FORCE_DX10_EXT_MISC2 = 0x20000,

        // Force use of legacy header for DDS writer (will fail if unable to write as such)
        DDS_FLAGS_FORCE_DX9_LEGACY = 0x40000,

        // Force use of 'RXGB' instead of 'DXT5' for DDS write of BC3_UNORM data
        DDS_FLAGS_FORCE_DXT5_RXGB = 0x80000,

        // Force use of 'RGB' 24bpp legacy Direct3D 9 format for DDS write of B8G8R8X8_UNORM data
        DDS_FLAGS_FORCE_24BPP_RGB = 0x100000,

        // Enables the loader to read large dimension .dds files (i.e. greater than known hardware requirements)
        DDS_FLAGS_ALLOW_LARGE_FILES = 0x1000000,
    };

    enum TGA_FLAGS : uint32_t
    {
        TGA_FLAGS_NONE = 0x0,

        // 24bpp files are returned as BGRX; 32bpp files are returned as BGRA
        TGA_FLAGS_BGR = 0x1,

        // If the loaded image has an all zero alpha channel, normally we assume it should be opaque. This flag leaves it alone.
        TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA = 0x2,

        // Ignores sRGB TGA 2.0 metadata if present in the file
        TGA_FLAGS_IGNORE_SRGB = 0x10,

        // Writes sRGB metadata into the file reguardless of format (TGA 2.0 only)
        TGA_FLAGS_FORCE_SRGB = 0x20,

        // Writes linear gamma metadata into the file reguardless of format (TGA 2.0 only)
        TGA_FLAGS_FORCE_LINEAR = 0x40,

        // If no colorspace is specified in TGA 2.0 metadata, assume sRGB
        TGA_FLAGS_DEFAULT_SRGB = 0x80,
    };

    enum WIC_FLAGS : uint32_t
    {
        WIC_FLAGS_NONE = 0x0,

        // Loads DXGI 1.1 BGR formats as VkFormat_R8G8B8A8_UNORM to avoid use of optional WDDM 1.1 formats
        WIC_FLAGS_FORCE_RGB = 0x1,

        // Loads DXGI 1.1 X2 10:10:10:2 format as VkFormat_R10G10B10A2_UNORM
        WIC_FLAGS_NO_X2_BIAS = 0x2,

        // Loads 565, 5551, and 4444 formats as 8888 to avoid use of optional WDDM 1.2 formats
        WIC_FLAGS_NO_16BPP = 0x4,

        // Loads 1-bit monochrome (black & white) as R1_UNORM rather than 8-bit grayscale
        WIC_FLAGS_ALLOW_MONO = 0x8,

        // Loads all images in a multi-frame file, converting/resizing to match the first frame as needed, defaults to 0th frame otherwise
        WIC_FLAGS_ALL_FRAMES = 0x10,

        // Ignores sRGB metadata if present in the file
        WIC_FLAGS_IGNORE_SRGB = 0x20,

        // Writes sRGB metadata into the file reguardless of format
        WIC_FLAGS_FORCE_SRGB = 0x40,

        // Writes linear gamma metadata into the file reguardless of format
        WIC_FLAGS_FORCE_LINEAR = 0x80,

        // If no colorspace is specified, assume sRGB
        WIC_FLAGS_DEFAULT_SRGB = 0x100,

        // Use ordered 4x4 dithering for any required conversions
        WIC_FLAGS_DITHER = 0x10000,

        // Use error-diffusion dithering for any required conversions
        WIC_FLAGS_DITHER_DIFFUSION = 0x20000,

        // Filtering mode to use for any required image resizing (only needed when loading arrays of differently sized images; defaults to Fant)
        WIC_FLAGS_FILTER_POINT = 0x100000,
        WIC_FLAGS_FILTER_LINEAR = 0x200000,
        WIC_FLAGS_FILTER_CUBIC = 0x300000,
        WIC_FLAGS_FILTER_FANT = 0x400000, // Combination of Linear and Box filter
    };

    // CP_FLAGS helper functions
    inline constexpr CP_FLAGS operator |(CP_FLAGS a, CP_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
        return static_cast<CP_FLAGS>(out);
    }

    inline CP_FLAGS &operator |=(CP_FLAGS &a, CP_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
        a = static_cast<CP_FLAGS>(out);
        return a;
    }

    inline constexpr CP_FLAGS operator &(CP_FLAGS a, CP_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) & static_cast<uint32_t>(b);
        return static_cast<CP_FLAGS>(out);
    }

    inline CP_FLAGS &operator &=(CP_FLAGS &a, CP_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) & static_cast<uint32_t>(b);
        a = static_cast<CP_FLAGS>(out);
        return a;
    }

    inline constexpr CP_FLAGS operator ~(CP_FLAGS a) noexcept
    {
        uint32_t out = ~static_cast<uint32_t>(a);
        return static_cast<CP_FLAGS>(out);
    }

    inline constexpr CP_FLAGS operator ^(CP_FLAGS a, CP_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b);
        return static_cast<CP_FLAGS>(out);
    }

    inline CP_FLAGS &operator ^=(CP_FLAGS &a, CP_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b);
        a = static_cast<CP_FLAGS>(out);
        return a;
    }

    // DDS_FLAGS helper functions
    inline constexpr DDS_FLAGS operator |(DDS_FLAGS a, DDS_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
        return static_cast<DDS_FLAGS>(out);
    }

    inline DDS_FLAGS &operator |=(DDS_FLAGS &a, DDS_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
        a = static_cast<DDS_FLAGS>(out);
        return a;
    }

    inline constexpr DDS_FLAGS operator &(DDS_FLAGS a, DDS_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) & static_cast<uint32_t>(b);
        return static_cast<DDS_FLAGS>(out);
    }

    inline DDS_FLAGS &operator &=(DDS_FLAGS &a, DDS_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) & static_cast<uint32_t>(b);
        a = static_cast<DDS_FLAGS>(out);
        return a;
    }

    inline constexpr DDS_FLAGS operator ~(DDS_FLAGS a) noexcept
    {
        uint32_t out = ~static_cast<uint32_t>(a);
        return static_cast<DDS_FLAGS>(out);
    }

    inline constexpr DDS_FLAGS operator ^(DDS_FLAGS a, DDS_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b);
        return static_cast<DDS_FLAGS>(out);
    }

    inline DDS_FLAGS &operator ^=(DDS_FLAGS &a, DDS_FLAGS b) noexcept
    {
        uint32_t out = static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b);
        a = static_cast<DDS_FLAGS>(out);
        return a;
    }

    //---------------------------------------------------------------------------------
    // Format Utilities
    bool IsValid(VkFormat fmt) noexcept;
    bool IsCompressed(VkFormat fmt) noexcept;
    bool IsPacked(VkFormat fmt) noexcept;
    bool IsVideo(VkFormat fmt) noexcept;
    bool IsPlanar(VkFormat fmt, bool isd3d12 = false) noexcept;
    bool IsPalettized(VkFormat fmt) noexcept;
    bool IsDepthStencil(VkFormat fmt) noexcept;
    bool IsSRGB(VkFormat fmt) noexcept;
    bool IsBGR(VkFormat fmt) noexcept;
    bool IsTypeless(VkFormat fmt, bool partialTypeless = true) noexcept;

    bool HasAlpha(VkFormat fmt) noexcept;

    size_t BitsPerPixel(VkFormat fmt) noexcept;

    size_t BitsPerColor(VkFormat fmt) noexcept;

    size_t BytesPerBlock(VkFormat fmt) noexcept;

    bool ComputePitch(
        VkFormat fmt, size_t width, size_t height,
        size_t& rowPitch, size_t& slicePitch, CP_FLAGS flags = CP_FLAGS_NONE) noexcept;

    size_t ComputeScanlines(VkFormat fmt, size_t height) noexcept;

    VkFormat MakeSRGB(VkFormat fmt) noexcept;

    struct TexMetadata
    {
        size_t          width;
        size_t          height;     // Should be 1 for 1D textures
        size_t          depth;      // Should be 1 for 1D or 2D textures
        size_t          arraySize;  // For cubemap, this is a multiple of 6
        size_t          mipLevels;
        uint32_t        miscFlags;
        uint32_t        miscFlags2;
        VkFormat        format;
        TEX_DIMENSION   dimension;

        // Returns size_t(-1) to indicate an out-of-range error
        size_t ComputeIndex(size_t mip, size_t item, size_t slice) const noexcept;

        // Helper for miscFlags
        bool IsCubemap() const noexcept { return (miscFlags & TEX_MISC_TEXTURECUBE) != 0; }

        bool IsPMAlpha() const noexcept { return ((miscFlags2 & TEX_MISC2_ALPHA_MODE_MASK) == TEX_ALPHA_MODE_PREMULTIPLIED) != 0; }
        void SetAlphaMode(TEX_ALPHA_MODE mode) noexcept { miscFlags2 = (miscFlags2 & ~static_cast<uint32_t>(TEX_MISC2_ALPHA_MODE_MASK)) | static_cast<uint32_t>(mode); }
        // Helpers for miscFlags2
        TEX_ALPHA_MODE GetAlphaMode() const noexcept { return static_cast<TEX_ALPHA_MODE>(miscFlags2 & TEX_MISC2_ALPHA_MODE_MASK); }

        // Helper for dimension
        bool IsVolumemap() const noexcept { return (dimension == TEX_DIMENSION_TEXTURE3D); }

        // Returns size_t(-1) to indicate an out-of-range error
        uint32_t CalculateSubresource(size_t mip, size_t item) const noexcept;
        uint32_t CalculateSubresource(size_t mip, size_t item, size_t plane) const noexcept;
    };

    struct DDSMetaData
    {
        uint32_t    size;           // DDPIXELFORMAT.dwSize
        uint32_t    flags;          // DDPIXELFORMAT.dwFlags
        uint32_t    fourCC;         // DDPIXELFORMAT.dwFourCC
        uint32_t    RGBBitCount;    // DDPIXELFORMAT.dwRGBBitCount/dwYUVBitCount/dwAlphaBitDepth/dwLuminanceBitCount/dwBumpBitCount
        uint32_t    RBitMask;       // DDPIXELFORMAT.dwRBitMask/dwYBitMask/dwLuminanceBitMask/dwBumpDuBitMask
        uint32_t    GBitMask;       // DDPIXELFORMAT.dwGBitMask/dwUBitMask/dwBumpDvBitMask
        uint32_t    BBitMask;       // DDPIXELFORMAT.dwBBitMask/dwVBitMask/dwBumpLuminanceBitMask
        uint32_t    ABitMask;       // DDPIXELFORMAT.dwRGBAlphaBitMask/dwYUVAlphaBitMask/dwLuminanceAlphaBitMask

        bool IsDX10() const noexcept { return (fourCC == 0x30315844); }
    };

    bool GetMetadataFromDDSMemory(
        const uint8_t* pSource, size_t size,
        DDS_FLAGS flags,
        TexMetadata& metadata) noexcept;
    bool GetMetadataFromDDSFile(
        const wchar_t* szFile,
        DDS_FLAGS flags,
        TexMetadata& metadata) noexcept;

    bool GetMetadataFromDDSMemoryEx(
        const uint8_t* pSource, size_t size,
        DDS_FLAGS flags,
        TexMetadata& metadata,
        DDSMetaData* ddPixelFormat) noexcept;
    bool GetMetadataFromDDSFileEx(
        const wchar_t* szFile,
        DDS_FLAGS flags,
        TexMetadata& metadata,
        DDSMetaData* ddPixelFormat) noexcept;

    bool GetMetadataFromTGAMemory(
        const uint8_t* pSource, size_t size,
        TGA_FLAGS flags,
        TexMetadata& metadata) noexcept;
    bool GetMetadataFromTGAFile(
        const wchar_t* szFile,
        TGA_FLAGS flags,
        TexMetadata& metadata) noexcept;

    // Compatability helpers
    bool GetMetadataFromTGAMemory(
        const uint8_t* pSource, size_t size,
        TexMetadata& metadata) noexcept;
    bool GetMetadataFromTGAFile(
        const wchar_t* szFile,
        TexMetadata& metadata) noexcept;

#ifdef __cpp_lib_byte
    bool GetMetadataFromDDSMemory(
        const std::byte* pSource, size_t size,
        DDS_FLAGS flags,
        TexMetadata& metadata) noexcept;
    bool GetMetadataFromDDSMemoryEx(
        const std::byte* pSource, size_t size,
        DDS_FLAGS flags,
        TexMetadata& metadata,
        DDSMetaData* ddPixelFormat) noexcept;
    bool GetMetadataFromHDRMemory(
        const std::byte* pSource, size_t size,
        TexMetadata& metadata) noexcept;
    bool GetMetadataFromTGAMemory(
        const std::byte* pSource, size_t size,
        TGA_FLAGS flags,
        TexMetadata& metadata) noexcept;
#endif // __cpp_lib_byte

    //---------------------------------------------------------------------------------
    // Bitmap image container
    struct Image
    {
        size_t   width;
        size_t   height;
        VkFormat format;
        size_t   rowPitch;
        size_t   slicePitch;
        uint8_t* pixels;
    };

    class ScratchImage
    {
    public:
        ScratchImage() noexcept
            : m_nimages(0), m_size(0), m_metadata{}, m_image(nullptr), m_memory(nullptr)
        {}
        ScratchImage(ScratchImage&& moveFrom) noexcept
            : m_nimages(0), m_size(0), m_metadata{}, m_image(nullptr), m_memory(nullptr)
        {
            *this = std::move(moveFrom);
        }
        ~ScratchImage() { Release(); }

        ScratchImage& operator= (ScratchImage&& moveFrom) noexcept;

        ScratchImage(const ScratchImage&) = delete;
        ScratchImage& operator=(const ScratchImage&) = delete;

        bool Initialize(const TexMetadata& mdata, CP_FLAGS flags = CP_FLAGS_NONE) noexcept;

        bool Initialize1D(VkFormat fmt, size_t length, size_t arraySize, size_t mipLevels, CP_FLAGS flags = CP_FLAGS_NONE) noexcept;
        bool Initialize2D(VkFormat fmt, size_t width, size_t height, size_t arraySize, size_t mipLevels, CP_FLAGS flags = CP_FLAGS_NONE) noexcept;
        bool Initialize3D(VkFormat fmt, size_t width, size_t height, size_t depth, size_t mipLevels, CP_FLAGS flags = CP_FLAGS_NONE) noexcept;
        bool InitializeCube(VkFormat fmt, size_t width, size_t height, size_t nCubes, size_t mipLevels, CP_FLAGS flags = CP_FLAGS_NONE) noexcept;

        bool InitializeFromImage(const Image& srcImage, bool allow1D = false, CP_FLAGS flags = CP_FLAGS_NONE) noexcept;
        bool InitializeArrayFromImages(const Image* images, size_t nImages, bool allow1D = false, CP_FLAGS flags = CP_FLAGS_NONE) noexcept;
        bool InitializeCubeFromImages(const Image* images, size_t nImages, CP_FLAGS flags = CP_FLAGS_NONE) noexcept;
        bool Initialize3DFromImages(const Image* images, size_t depth, CP_FLAGS flags = CP_FLAGS_NONE) noexcept;

        void Release() noexcept;

        bool OverrideFormat(VkFormat f) noexcept;

        const TexMetadata& GetMetadata() const noexcept { return m_metadata; }
        const Image* GetImage(size_t mip, size_t item, size_t slice) const noexcept;

        const Image* GetImages() const noexcept { return m_image; }
        size_t GetImageCount() const noexcept { return m_nimages; }

        uint8_t* GetPixels() const noexcept { return m_memory; }
        size_t GetPixelsSize() const noexcept { return m_size; }

    private:
        size_t      m_nimages;
        size_t      m_size;
        TexMetadata m_metadata;
        Image*      m_image;
        uint8_t*    m_memory;
    };

    //---------------------------------------------------------------------------------
    // Memory blob (allocated buffer pointer is always 16-byte aligned)
    class Blob
    {
    public:
        Blob() noexcept : m_buffer(nullptr), m_size(0) {}
        Blob(Blob&& moveFrom) noexcept : m_buffer(nullptr), m_size(0) { *this = std::move(moveFrom); }
        ~Blob() { Release(); }

        Blob& operator= (Blob&& moveFrom) noexcept;

        Blob(const Blob&) = delete;
        Blob& operator=(const Blob&) = delete;

        bool Initialize(size_t size) noexcept;

        void Release() noexcept;

        uint8_t* GetBufferPointer() const noexcept { return m_buffer; }

        const uint8_t* GetConstBufferPointer() const noexcept { return m_buffer; }

        size_t GetBufferSize() const noexcept { return m_size; }

        // Reallocate for a new size
        bool Resize(size_t size) noexcept;

        // Shorten size without reallocation
        bool Trim(size_t size) noexcept;

    private:
        uint8_t* m_buffer;
        size_t   m_size;
    };

    //---------------------------------------------------------------------------------
    // Image I/O
    // DDS operations
    bool LoadFromDDSMemory(
        const uint8_t* pSource, size_t size,
        DDS_FLAGS flags,
        TexMetadata* metadata, ScratchImage& image) noexcept;
    bool LoadFromDDSFile(
        const wchar_t* szFile,
        DDS_FLAGS flags,
        TexMetadata* metadata, ScratchImage& image) noexcept;

    bool LoadFromDDSMemoryEx(
        const uint8_t* pSource, size_t size,
        DDS_FLAGS flags,
        TexMetadata* metadata,
        DDSMetaData* ddPixelFormat,
        ScratchImage& image) noexcept;
    bool LoadFromDDSFileEx(
        const wchar_t* szFile,
        DDS_FLAGS flags,
        TexMetadata* metadata,
        DDSMetaData* ddPixelFormat,
        ScratchImage& image) noexcept;

    bool SaveToDDSMemory(
        const Image& image,
        DDS_FLAGS flags,
        Blob& blob) noexcept;
    bool SaveToDDSMemory(
        const Image* images, size_t nimages, const TexMetadata& metadata,
        DDS_FLAGS flags,
        Blob& blob) noexcept;

    bool SaveToDDSFile(const Image& image, DDS_FLAGS flags, const char* szFile) noexcept;
    bool SaveToDDSFile(
        const Image* images, size_t nimages, const TexMetadata& metadata,
        DDS_FLAGS flags, const char* szFile) noexcept;

    // TGA operations
    bool LoadFromTGAMemory(
        const uint8_t* pSource, size_t size,
        TGA_FLAGS flags,
        TexMetadata* metadata, ScratchImage& image) noexcept;
    bool LoadFromTGAFile(
        const wchar_t* szFile,
        TGA_FLAGS flags,
        TexMetadata* metadata, ScratchImage& image) noexcept;

    bool SaveToTGAMemory(const Image& image,
        TGA_FLAGS flags,
        Blob& blob, const TexMetadata* metadata = nullptr) noexcept;
    bool SaveToTGAFile(const Image& image, TGA_FLAGS flags,
                        const char* szFile, const TexMetadata* metadata = nullptr) noexcept;

    // Compatability helpers
    bool LoadFromTGAMemory(
        const uint8_t* pSource, size_t size,
        TexMetadata* metadata, ScratchImage& image) noexcept;
    bool LoadFromTGAFile(
        const wchar_t* szFile,
        TexMetadata* metadata, ScratchImage& image) noexcept;

    bool SaveToTGAMemory(const Image& image, Blob& blob, const TexMetadata* metadata = nullptr) noexcept;
    bool SaveToTGAFile(const Image& image, const char* szFile, const TexMetadata* metadata = nullptr) noexcept;

#ifdef __cpp_lib_byte
    bool LoadFromDDSMemory(
        const std::byte* pSource, size_t size,
        DDS_FLAGS flags,
        TexMetadata* metadata, ScratchImage& image) noexcept;
    bool LoadFromDDSMemoryEx(
        const std::byte* pSource, size_t size,
        DDS_FLAGS flags,
        TexMetadata* metadata,
        DDSMetaData* ddPixelFormat,
        ScratchImage& image) noexcept;
    bool LoadFromTGAMemory(
        const std::byte* pSource, size_t size,
        TGA_FLAGS flags,
        TexMetadata* metadata, ScratchImage& image) noexcept;
#endif // __cpp_lib_byte

    //---------------------------------------------------------------------------------
    // DDS helper functions
    bool EncodeDDSHeader(
        const TexMetadata& metadata, DDS_FLAGS flags,
        uint8_t* pDestination, size_t maxsize,
        size_t& required) noexcept;

#ifdef __cpp_lib_byte
    bool EncodeDDSHeader(
        const TexMetadata& metadata, DDS_FLAGS flags,
        std::byte* pDestination, size_t maxsize,
        size_t& required) noexcept;

    bool EncodeDDSHeader(
        const TexMetadata& metadata, DDS_FLAGS flags,
        std::nullptr_t, size_t maxsize,
        size_t& required) noexcept;
#endif
} // namespace VulkanTex

namespace VulkanTex
{
    //---------------------------------------------------------------------------------
    // Image helper functions
    bool DetermineImageArray(
        const TexMetadata& metadata, CP_FLAGS cpFlags,
        size_t& nImages, size_t& pixelSize) noexcept;

    bool SetupImageArray(
        uint8_t* pMemory, size_t pixelSize,
        const TexMetadata& metadata, CP_FLAGS cpFlags,
        Image* images, size_t nImages) noexcept;

    //---------------------------------------------------------------------------------
    // Conversion helper functions
    enum TEXP_SCANLINE_FLAGS : uint32_t
    {
        TEXP_SCANLINE_NONE = 0,

        TEXP_SCANLINE_SETALPHA = 0x1,
        // Set alpha channel to known opaque value

        TEXP_SCANLINE_LEGACY = 0x2,
        // Enables specific legacy format conversion cases
    };

    enum CONVERT_FLAGS : uint32_t
    {
        CONVF_FLOAT = 0x1,
        CONVF_UNORM = 0x2,
        CONVF_UINT = 0x4,
        CONVF_SNORM = 0x8,
        CONVF_SINT = 0x10,
        CONVF_DEPTH = 0x20,
        CONVF_STENCIL = 0x40,
        CONVF_SHAREDEXP = 0x80,
        CONVF_BGR = 0x100,
        CONVF_XR = 0x200,
        CONVF_PACKED = 0x400,
        CONVF_BC = 0x800,
        CONVF_YUV = 0x1000,
        CONVF_POS_ONLY = 0x2000,
        CONVF_R = 0x10000,
        CONVF_G = 0x20000,
        CONVF_B = 0x40000,
        CONVF_A = 0x80000,
        CONVF_RGB_MASK = 0x70000,
        CONVF_RGBA_MASK = 0xF0000,
    };

    uint32_t GetConvertFlags(VkFormat format) noexcept;

    void CopyScanline(
        void* pDestination, size_t outSize,
        const void* pSource, size_t inSize,
        VkFormat format, uint32_t tflags) noexcept;

    void SwizzleScanline(
        void* pDestination, size_t outSize,
        const void* pSource, size_t inSize,
        VkFormat format, uint32_t tflags) noexcept;

    bool ExpandScanline(
        void* pDestination, size_t outSize,
        VkFormat outFormat,
        const void* pSource, size_t inSize,
        VkFormat inFormat, uint32_t tflags) noexcept;

    bool ConvertToR32G32B32A32(const Image& srcImage, ScratchImage& image) noexcept;

    bool ConvertFromR32G32B32A32(const Image& srcImage, const Image& destImage) noexcept;
    bool ConvertFromR32G32B32A32(
        const Image& srcImage, VkFormat format, ScratchImage& image) noexcept;
    bool ConvertFromR32G32B32A32(
        const Image* srcImages, size_t nimages, const TexMetadata& metadata,
        VkFormat format, ScratchImage& result) noexcept;

    bool ConvertToR16G16B16A16(const Image& srcImage, ScratchImage& image) noexcept;

    bool ConvertFromR16G16B16A16(const Image& srcImage, const Image& destImage) noexcept;

    //---------------------------------------------------------------------------------
    // Misc helper functions
    bool IsAlphaAllOpaqueBC(const Image& cImage) noexcept;
    bool CalculateMipLevels(size_t width, size_t height, size_t& mipLevels) noexcept;
    bool CalculateMipLevels3D(size_t width, size_t height, size_t depth,
        size_t& mipLevels) noexcept;

} // namespace VulkanTex
