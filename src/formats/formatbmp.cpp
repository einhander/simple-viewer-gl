/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "formatbmp.h"
#include "../common/bitmap_description.h"
#include "../common/file.h"
#include "../common/helpers.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{

#pragma pack(push, 1)
    struct BmpHeader
    {
        uint8_t id[2];
        uint32_t fileSize;
        uint16_t reserved[2];
        uint32_t bitmapOffset;
    };
#pragma pack(pop)

    // -------------------------------------------------------------------------
    // OS/2 1.x
    // -------------------------------------------------------------------------

#pragma pack(push, 1)
    struct BITMAPCOREHEADER
    {
        uint32_t size;         // size of this header (12 bytes)
        uint16_t width;        // bitmap width in pixels (unsigned 16-bit)
        uint16_t height;       // bitmap height in pixels (unsigned 16-bit)
        uint16_t planes;       // number of color planes, must be 1
        uint16_t bitCount;     // number of bits per pixel OS/2 1.x bitmaps are uncompressed and cannot be 16 or 32 bpp
    };
#pragma pack(pop)

    // -------------------------------------------------------------------------
    // Windows
    // -------------------------------------------------------------------------

#pragma pack(push, 1)
    struct BITMAPCOMMON
    {
        uint32_t size;            // size of this header (40 bytes);
        uint32_t width;           // bitmap width in pixels (signed integer)
        uint32_t height;          // bitmap height in pixels (signed integer)
        uint16_t planes;          // number of color planes (must be 1)
        uint16_t bitCount;        // number of bits per pixel, which is the color depth of the image. Typical values are 1, 4, 8, 16, 24 and 32.
        uint32_t compression;     // compression method being used. See the next table for a list of possible values
        uint32_t sizeImage;       // image size. This is the size of the raw bitmap data; a dummy 0 can be given for BI_RGB bitmaps.;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    // size of this header (40 bytes)
    struct BITMAPINFOHEADER : BITMAPCOMMON
    {
        uint32_t horiResolution;  // horizontal resolution of the image. (pixel per meter, signed integer)
        uint32_t vertResolution;  // vertical resolution of the image. (pixel per meter, signed integer)
        uint32_t paletteColors;   // number of colors in the color palette, or 0 to default to 2n
        uint32_t importantColors; // number of important colors used, or 0 when every color is important; generally ignored
    };
#pragma pack(pop)

    typedef int32_t FXPT2DOT30;

    struct Xyz // CIEXYZ
    {
        int32_t x; //FXPT2DOT30 x;
        int32_t y; //FXPT2DOT30 y;
        int32_t z; //FXPT2DOT30 z;
    };

    struct XyzTriple // CIEXYZTRIPLE
    {
        Xyz r; // CIEXYZ red;
        Xyz g; // CIEXYZ green;
        Xyz b; // CIEXYZ blue;
    };

#pragma pack(push, 1)
    // size of this header (108 bytes)
    struct BITMAPV4HEADER : BITMAPCOMMON
    {
        uint32_t  xPelsPerMeter;
        uint32_t  yPelsPerMeter;
        uint32_t  clrUsed;
        uint32_t  clrImportant;
        uint32_t  redMask;
        uint32_t  greenMask;
        uint32_t  blueMask;
        uint32_t  alphaMask;
        uint32_t  cSType;
        XyzTriple endpoints; // CIEXYZTRIPLE endpoints;
        uint32_t  gammaRed;
        uint32_t  gammaGreen;
        uint32_t  gammaBlue;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    // size of this header (124 bytes)
    struct BITMAPV5HEADER : BITMAPV4HEADER
    {
        uint32_t  intent;
        uint32_t  profileData;
        uint32_t  profileSize;
        uint32_t  reserved;
    };
#pragma pack(pop)

    enum class Version
    {
        Core,

        Info,
        V4,
        V5,

        Unknown,
    };


    Version getVersion(unsigned headerSize)
    {
        if (headerSize == sizeof(BITMAPCOREHEADER))
        {
            return Version::Core;
        }
        else if (headerSize == sizeof(BITMAPINFOHEADER))
        {
            return Version::Info;
        }
        else if (headerSize == sizeof(BITMAPV4HEADER))
        {
            return Version::V4;
        }
        else if (headerSize == sizeof(BITMAPV5HEADER))
        {
            return Version::V5;
        }

        return Version::Unknown;
    }

    enum class Compression
    {
        BI_RGB            = 0,  // none                           Most common
        BI_RLE8           = 1,  // RLE 8-bit/pixel                Can be used only with 8-bit/pixel bitmaps
        BI_RLE4           = 2,  // RLE 4-bit/pixel                Can be used only with 4-bit/pixel bitmaps
        BI_BITFIELDS      = 3,  // OS22XBITMAPHEADER: Huffman 1D  BITMAPV2INFOHEADER: RGB bit field masks,
        //                                BITMAPV3INFOHEADER+: RGBA
        BI_JPEG           = 4,  // OS22XBITMAPHEADER: RLE-24      BITMAPV4INFOHEADER+: JPEG image for printing[12]
        BI_PNG            = 5,  //                                BITMAPV4INFOHEADER+: PNG image for printing[12]
        BI_ALPHABITFIELDS = 6,  // RGBA bit field masks           only Windows CE 5.0 with .NET 4.0 or later
        BI_CMYK           = 11, // none                           only Windows Metafile CMYK[3]
        BI_CMYKRLE8       = 12, // RLE-8                          only Windows Metafile CMYK
        BI_CMYKRLE4       = 13, // RLE-4                          only Windows Metafile CMYK
    };

    const char* compressionToName(uint32_t compression)
    {
        const char* Names[] =
        {
            "BI_RGB",
            "BI_RLE8",
            "BI_RLE4",
            "BI_BITFIELDS",
            "BI_JPEG",
            "BI_PNG",
            "BI_ALPHABITFIELDS",
            "n/a",
            "n/a",
            "n/a",
            "n/a",
            "BI_CMYK",
            "BI_CMYKRLE8",
            "BI_CMYKRLE4",
        };

        return compression < helpers::countof(Names) ? Names[compression] : "n/a";
    }

    void debugHeader(const BITMAPCOREHEADER& header)
    {
        auto ver = getVersion(header.size);
        if (ver == Version::Core)
        {
            ::printf("-- BITMAPCOREHEADER\n");
            ::printf("     header size   : %u\n", header.size);
            ::printf("     image width   : %u\n", (uint32_t)header.width);
            ::printf("     image height  : %u\n", (uint32_t)header.height);
            ::printf("     planes        : %u\n", (uint32_t)header.planes);
            ::printf("     bitCount      : %u\n", (uint32_t)header.bitCount);
        }
    }

    void debugHeader(const BITMAPCOMMON& header)
    {
        ::printf("-- BITMAPCOMMON\n");
        ::printf("     header size   : %u\n", header.size);
        ::printf("     image width   : %u\n", header.width);
        ::printf("     image height  : %u\n", header.height);
        ::printf("     planes        : %u\n", header.planes);
        ::printf("     bitCount      : %u\n", header.bitCount);
        ::printf("     compression   : %s\n", compressionToName(header.compression));
        ::printf("     sizeImage     : %u\n", header.sizeImage);

        auto ver = getVersion(header.size);
        if (ver == Version::Info)
        {
            auto& h = reinterpret_cast<const BITMAPINFOHEADER&>(header);
            ::printf("-- BITMAPINFOHEADER\n");
            ::printf("     horiResolution  : %u\n", h.horiResolution);
            ::printf("     vertResolution  : %u\n", h.vertResolution);
            ::printf("     paletteColors   : %u\n", h.paletteColors);
            ::printf("     importantColors : %u\n", h.importantColors);
        }
        else
        {
            if (ver == Version::V4 || ver == Version::V5)
            {
                auto& h = reinterpret_cast<const BITMAPV4HEADER&>(header);
                ::printf("-- BITMAPV4HEADER\n");
                ::printf("     xPelsPerMeter : %u\n", h.xPelsPerMeter);
                ::printf("     yPelsPerMeter : %u\n", h.yPelsPerMeter);
                ::printf("     clrUsed       : %u\n", h.clrUsed);
                ::printf("     clrImportant  : %u\n", h.clrImportant);
                ::printf("     redMask       : %u\n", h.redMask);
                ::printf("     greenMask     : %u\n", h.greenMask);
                ::printf("     blueMask      : %u\n", h.blueMask);
                ::printf("     alphaMask     : %u\n", h.alphaMask);
                ::printf("     cSType        : %u\n", h.cSType);
                ::printf("     endp red      : %d %d %d\n", h.endpoints.r.x, h.endpoints.r.y, h.endpoints.r.z);
                ::printf("     endp green    : %d %d %d\n", h.endpoints.g.x, h.endpoints.g.y, h.endpoints.g.z);
                ::printf("     endp blue     : %d %d %d\n", h.endpoints.b.x, h.endpoints.b.y, h.endpoints.b.z);
                ::printf("     gammaRed      : %u\n", h.gammaRed);
                ::printf("     gammaGreen    : %u\n", h.gammaGreen);
                ::printf("     gammaBlue     : %u\n", h.gammaBlue);
            }

            if (ver == Version::V5)
            {
                auto& h = reinterpret_cast<const BITMAPV5HEADER&>(header);
                ::printf("-- BITMAPV5HEADER\n");
                ::printf("     intent        : %u\n", h.intent);
                ::printf("     profileData   : %u\n", h.profileData);
                ::printf("     profileSize   : %u\n", h.profileSize);
            }
        }
    }

    bool isValidFormat(const BmpHeader& header, uint32_t size)
    {
        // ::printf("-- file size: %u\n", size);
        // ::printf("-- bmp  size: %u\n", header.fileSize);

        const bool idValid =
            (header.id[0] == 'B' && header.id[1] == 'M') || // BM – Windows 3.1x, 95, NT, ... etc.
            (header.id[0] == 'B' && header.id[1] == 'A') || // BA – OS/2 struct bitmap array
            (header.id[0] == 'C' && header.id[1] == 'I') || // CI – OS/2 struct color icon
            (header.id[0] == 'C' && header.id[1] == 'P') || // CP – OS/2 const color pointer
            (header.id[0] == 'I' && header.id[1] == 'C') || // IC – OS/2 struct icon
            (header.id[0] == 'P' && header.id[1] == 'T');   // PT – OS/2 pointer
        return idValid && size == header.fileSize && header.bitmapOffset < size;
    }

    void setGLformat(uint32_t bitCount, sBitmapDescription& desc)
    {
        desc.bppImage = bitCount;

        switch (bitCount)
        {
        case 1:
            desc.format = GL_LUMINANCE;
            desc.bpp = 8;
            break;

        case 8:
            desc.format = GL_BGR;
            desc.bpp = 24;
            break;

        case 16:
            desc.format = GL_UNSIGNED_SHORT_5_6_5;
            desc.bpp = 16;
            break;

        case 32:
            desc.format = GL_BGRA;
            desc.bpp = 32;
            break;

        default:
            desc.format = GL_BGR;
            desc.bpp = 24;
        }
    }

    bool readUncompressed1(cFile& file, sBitmapDescription& desc, const BITMAPCOMMON& header)
    {
        const uint32_t inPitch = header.sizeImage / desc.height;
        Buffer buffer(inPitch);

        uint8_t c0 = 0;
        uint8_t c1 = 255;

        auto in = buffer.data();
        for (uint32_t row = 0; row < desc.height; row++)
        {
            if (inPitch != file.read(in, inPitch))
            {
                return false;
            }

            auto out = desc.bitmap.data() + (desc.height - row - 1) * desc.pitch;
            size_t idx = 0;
            for (uint32_t i = 0; i < inPitch; i++)
            {
                const auto byte = in[i];
                for (uint32_t b = 0; b < 8 && idx < desc.width; b++, idx++)
                {
                    const uint32_t bit = 0x80 >> b;
                    const uint8_t val = (byte & bit) != 0 ? c1 : c0;
                    out[idx] = val;
                }
            }
        }

        return true;
    }

    bool readUncompressed8(cFile& file, sBitmapDescription& desc, const BITMAPCOMMON& header)
    {
        const uint32_t inPitch = header.sizeImage / desc.height;
        Buffer buffer(inPitch);

        auto in = buffer.data();
        for (uint32_t row = 0; row < desc.height; row++)
        {
            if (inPitch != file.read(in, inPitch))
            {
                return false;
            }

            auto out = desc.bitmap.data() + (desc.height - row - 1) * desc.pitch;
            size_t idx = 0;
            for (uint32_t i = 0; i < inPitch; i++)
            {
                const auto byte = in[i];
                out[idx++] = byte;
                out[idx++] = byte;
                out[idx++] = byte;
            }
        }

        return true;
    }

    bool readUncompressed16(cFile& file, sBitmapDescription& desc)
    {
        for (uint32_t row = 0; row < desc.height; row++)
        {
            auto out = desc.bitmap.data() + (desc.height - row - 1) * desc.pitch;
            if (file.read(out, desc.pitch) != desc.pitch)
            {
                return false;
            }
        }
        return true;
    }

    bool readUncompressed24(cFile& file, sBitmapDescription& desc)
    {
        for (uint32_t row = 0; row < desc.height; row++)
        {
            auto out = desc.bitmap.data() + (desc.height - row - 1) * desc.pitch;
            if (file.read(out, desc.pitch) != desc.pitch)
            {
                return false;
            }
        }
        return true;
    }

    bool readUncompressed32(cFile& file, sBitmapDescription& desc)
    {
        for (uint32_t row = 0; row < desc.height; row++)
        {
            auto out = desc.bitmap.data() + (desc.height - row - 1) * desc.pitch;
            if (file.read(out, desc.pitch) != desc.pitch)
            {
                return false;
            }
        }
        return true;
    }

}

cFormatBmp::cFormatBmp(iCallbacks* callbacks)
    : cFormat(callbacks)
{
}

cFormatBmp::~cFormatBmp()
{
}

bool cFormatBmp::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, sizeof(BmpHeader)))
    {
        return false;
    }

    auto bmpHeader = reinterpret_cast<const BmpHeader*>(buffer.data());
    return isValidFormat(*bmpHeader, file.getSize());
}

bool cFormatBmp::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (!file.open(filename))
    {
        return false;
    }

    // ::printf("-- BITMAPCOREHEADER : %u\n", (uint32_t)sizeof(BITMAPCOREHEADER));
    // ::printf("-- BITMAPCOMMON     : %u\n", (uint32_t)sizeof(BITMAPCOMMON));
    // ::printf("-- BITMAPINFOHEADER : %u\n", (uint32_t)sizeof(BITMAPINFOHEADER));
    // ::printf("-- BITMAPV4HEADER   : %u\n", (uint32_t)sizeof(BITMAPV4HEADER));
    // ::printf("-- BITMAPV5HEADER   : %u\n", (uint32_t)sizeof(BITMAPV5HEADER));

    BmpHeader bmpHeader;
    if (file.read(&bmpHeader, sizeof(bmpHeader)) != sizeof(bmpHeader))
    {
        ::printf("(EE) Can't read BMP header.\n");
        return false;
    }

    if (isValidFormat(bmpHeader, file.getSize()) == false)
    {
        ::printf("(EE) Invalid BMP header.\n");
        return false;
    }

    desc.size = file.getSize();

    auto offset = file.getOffset();
    BITMAPCOMMON common;
    if (file.read(&common, sizeof(common)) != sizeof(common))
    {
        ::printf("(EE) Can't read DIB header size.\n");
        return false;
    }
    file.seek(offset, SEEK_SET);

    auto ver = getVersion(common.size);

    if (ver == Version::Info
        || ver == Version::V4
        || ver == Version::V5)
    {
        BITMAPV5HEADER header;
        if (file.read(&header, common.size) != common.size)
        {
            ::printf("(EE) Can't read DIB header.\n");
            return false;
        }

        debugHeader(header);

        desc.width = header.width;
        desc.height = header.height;

        setGLformat(header.bitCount, desc);

        desc.pitch = helpers::calculatePitch(desc.width, desc.bpp);
        desc.bitmap.resize(desc.pitch * desc.height);

        if ((Compression)header.compression == Compression::BI_RGB
            || (Compression)header.compression == Compression::BI_BITFIELDS)
        {
            file.seek(bmpHeader.bitmapOffset, SEEK_SET);

            bool result = false;

            switch (header.bitCount)
            {
            case 1:
                result = readUncompressed1(file, desc, header);
                break;

            case 8:
                result = readUncompressed8(file, desc, header);
                break;

            case 16:
                result = readUncompressed16(file, desc);
                break;

            case 24:
                result = readUncompressed24(file, desc);
                break;

            case 32:
                result = readUncompressed32(file, desc);
                break;
            }

            if (result == false)
            {
                ::printf("(EE) Can't read bitmap data.\n");
                return false;
            }
        }
        else
        {
            ::printf("(EE) Unsupported compression : %s\n", compressionToName(header.compression));
            return false;
        }
    }
    else
    {
        if (ver == Version::Core)
        {
            BITMAPCOREHEADER header;
            if (file.read(&header, sizeof(header)) != sizeof(header))
            {
                ::printf("(EE) Can't read DIB header.\n");
                return false;
            }

            debugHeader(header);
        }

        ::printf("(EE) Unsupported BMP header.\n");

        return false;
    }

    m_formatName = "bmp";

    return true;
}
