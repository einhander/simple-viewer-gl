/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "formatico.h"
#include "../common/bitmap_description.h"
#include "../common/file.h"

#include <cstring>
#include <cmath>
#include <png.h>

#pragma pack(push, 1)
struct IcoHeader
{
    uint16_t reserved;	// Reserved. Should always be 0.
    uint16_t type;		// Specifies image type: 1 for icon (.ICO) image, 2 for cursor (.CUR) image. Other values are invalid.
    uint16_t count;		// Specifies number of images in the file.
};

// List of icons.
// Size = IcoHeader.ount * 16
struct IcoDirentry
{
    uint8_t width;	// Specifies image width in pixels. Can be 0, 255 or a number between 0 to 255. Should be 0 if image width is 256 pixels.
    uint8_t height;	// Specifies image height in pixels. Can be 0, 255 or a number between 0 to 255. Should be 0 if image height is 256 pixels.
    uint8_t colors;	// Specifies number of colors in the color palette. Should be 0 if the image is truecolor.
    uint8_t reserved;	// Reserved. Should be 0.[Notes 1]
    uint16_t planes;	// In .ICO format: Specifies color planes. Should be 0 or 1.
    // In .CUR format: Specifies the horizontal coordinates of the hotspot in number of pixels from the left.
    uint16_t bits;	// In .ICO format: Specifies bits per pixel. (1, 4, 8)
    // In .CUR format: Specifies the vertical coordinates of the hotspot in number of pixels from the top.
    uint32_t size;	// Specifies the size of the bitmap data in bytes. Size of (InfoHeader + ANDbitmap + XORbitmap)
    uint32_t offset;	// Specifies the offset of bitmap data address in the file
};

// Variant of BMP InfoHeader.
// Size = 40 bytes.
struct IcoBmpInfoHeader
{
    uint32_t size;      // Size of InfoHeader structure = 40
    uint32_t width;     // Icon Width
    uint32_t height;    // Icon Height (added height of XOR-Bitmap and AND-Bitmap)
    uint16_t planes;    // number of planes = 1
    uint16_t bits;      // bits per pixel = 1, 2, 4, 8, 16, 24, 32
    uint32_t reserved0; // Type of Compression = 0
    uint32_t imagesize; // Size of Image in Bytes = 0 (uncompressed)
    uint32_t reserved1; // XpixelsPerM
    uint32_t reserved2; // YpixelsPerM
    uint32_t reserved3; // ColorsUsed
    uint32_t reserved4; // ColorsImportant
};

// Color Map for XOR-Bitmap.
// Size = NumberOfColors * 4 bytes.
struct IcoColors
{
    uint8_t red;      // red component
    uint8_t green;    // green component
    uint8_t blue;     // blue component
    uint8_t reserved; // = 0
};

// uint8_t xormask; // DIB bits for XOR mask
// uint8_t andmask; // DIB bits for AND mask
#pragma pack(pop)

struct PngRaw
{
    uint8_t* data;
    size_t size;	// size of raw data
    size_t pos;	// current pos
};


// load frame in png format
static PngRaw m_pngRaw;
static void readPngData(png_structp /*png*/, png_bytep out, png_size_t count)
{
    // PngRaw& pngRaw	= *(PngRaw*)png->io_ptr;
    if(m_pngRaw.pos + count <= m_pngRaw.size)
    {
        memcpy((uint8_t*)out, &m_pngRaw.data[m_pngRaw.pos], count);
    }
    m_pngRaw.pos += count;
}


CFormatIco::CFormatIco(const char* lib, const char* name, iCallbacks* callbacks)
    : CFormat(lib, name, callbacks)
{
}

CFormatIco::~CFormatIco()
{
}

bool CFormatIco::Load(const char* filename, sBitmapDescription& desc)
{
    m_filename = filename;
    return load(0, desc);
}

bool CFormatIco::LoadSubImage(unsigned subImage, sBitmapDescription& desc)
{
    return load(subImage, desc);
}

bool CFormatIco::load(unsigned subImage, sBitmapDescription& desc)
{
    cFile file;
    if(!file.open(m_filename.c_str()))
    {
        return false;
    }

    desc.size = file.getSize();

    IcoHeader header;
    if(sizeof(header) != file.read(&header, sizeof(header)))
    {
        return false;
    }

    IcoDirentry* images	= new IcoDirentry[header.count];
    if(sizeof(IcoDirentry) * header.count != file.read(images, sizeof(IcoDirentry) * header.count))
    {
        delete[] images;
        return false;
    }

    subImage = std::max<unsigned>(subImage, 0);
    subImage = std::min<unsigned>(subImage, header.count - 1);
    IcoDirentry* image = &images[subImage];
    //	std::cout << std::endl;
    //	std::cout << "--- IcoDirentry ---" << std::endl;
    //	std::cout << "width: " << (int)image->width << "." << std::endl;
    //	std::cout << "height: " << (int)image->height << "." << std::endl;
    //	std::cout << "colors: " << (int)image->colors << "." << std::endl;
    //	std::cout << "planes: " << (int)image->planes << "." << std::endl;
    //	std::cout << "bits: " << (int)image->bits << "." << std::endl;
    //	std::cout << "size: " << (int)image->size << "." << std::endl;
    //	std::cout << "offset: " << (int)image->offset << "." << std::endl;

    bool result = false;

    if(image->colors == 0 && image->width == 0 && image->height == 0)
    {
        result = loadPngFrame(desc, file, image);
    }
    else
    {
        result = loadOrdinaryFrame(desc, file, image);
    }

    delete[] images;

    // store frame number and frames count after reset again
    desc.subImage = subImage;
    desc.subCount = header.count;

    return result;
}

bool CFormatIco::loadPngFrame(sBitmapDescription& desc, cFile& file, const IcoDirentry* image)
{
    uint8_t* p = new uint8_t[image->size];

    file.seek(image->offset, SEEK_SET);
    if(image->size != file.read(p, image->size))
    {
        delete[] p;
        return false;
    }

    if(image->size != 8 && png_sig_cmp(p, 0, 8) != 0)
    {
        printf("Frame is not recognized as a PNG format\n");
        delete[] p;
        return false;
    }

    // initialize stuff
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(png == 0)
    {
        printf("png_create_read_struct failed\n");
        delete[] p;
        return false;
    }

    m_pngRaw.data = p;
    m_pngRaw.size = image->size;
    m_pngRaw.pos = 8;
    png_set_read_fn(png, &m_pngRaw, readPngData);

    png_infop info = png_create_info_struct(png);
    if(info == 0)
    {
        printf("png_create_info_struct failed\n");
        delete[] p;
        return false;
    }

    if(setjmp(png_jmpbuf(png)) != 0)
    {
        printf("Error during init_io\n");
        delete[] p;
        return false;
    }

    png_init_io(png, (FILE*)file.getHandle());
    png_set_sig_bytes(png, 8);

    png_read_info(png, info);

    // get real bits per pixel
    desc.bppImage = png_get_bit_depth(png, info) * png_get_channels(png, info);

    //if(info->color_type == PNG_COLOR_TYPE_PALETTE)
    uint8_t color_type = png_get_color_type(png, info);
    if(color_type == PNG_COLOR_TYPE_PALETTE)
    {
        png_set_palette_to_rgb(png);
    }

#if defined(PNG_1_0_X) || defined (PNG_1_2_X)
    if(info->color_type == PNG_COLOR_TYPE_GRAY && info->bit_depth < 8)
    {
        // depreceted in libPNG-1.4.2
        png_set_gray_1_2_4_to_8(png);
    }
#endif

    if(png_get_valid(png, info, PNG_INFO_tRNS))
    {
        png_set_tRNS_to_alpha(png);
    }
    if(png_get_bit_depth(png, info) == 16)
    {
        png_set_strip_16(png);
    }
    if(color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
        png_set_gray_to_rgb(png);
    }

    //	int number_of_passes	= png_set_interlace_handling(png);
    png_read_update_info(png, info);

    desc.width = png_get_image_width(png, info);
    desc.height =png_get_image_height(png, info);
    desc.pitch = png_get_rowbytes(png, info);
    desc.bpp = png_get_bit_depth(png, info) * png_get_channels(png, info);

    // read file
    if(setjmp(png_jmpbuf(png)) != 0)
    {
        printf("Error during read_image\n");
        delete[] p;
        return false;
    }

    // create buffer and read data
    png_bytep* row_pointers = new png_bytep[desc.height];
    for(unsigned y = 0; y < desc.height; y++)
    {
        row_pointers[y] = new png_byte[desc.pitch];
    }
    png_read_image(png, row_pointers);

    // create BGRA buffer and decode image data
    desc.bitmap.resize(desc.pitch * desc.height);

    color_type = png_get_color_type(png, info);
    if(color_type == PNG_COLOR_TYPE_RGB)
    {
        desc.format = GL_RGB;
        for(unsigned y = 0; y < desc.height; y++)
        {
            unsigned dst = y * desc.pitch;
            for(unsigned x = 0; x < desc.width; x++)
            {
                unsigned dx = x * 3;
                desc.bitmap[dst + dx + 0] = *(row_pointers[y] + dx + 0);
                desc.bitmap[dst + dx + 1] = *(row_pointers[y] + dx + 1);
                desc.bitmap[dst + dx + 2] = *(row_pointers[y] + dx + 2);
            }

            updateProgress((float)y / desc.height);

            delete[] row_pointers[y];
        }
    }
    else if(color_type == PNG_COLOR_TYPE_RGB_ALPHA)
    {
        desc.format = GL_RGBA;
        for(unsigned y = 0; y < desc.height; y++)
        {
            unsigned dst = y * desc.pitch;
            for(unsigned x = 0; x < desc.width; x++)
            {
                unsigned dx = x * 4;
                desc.bitmap[dst + dx + 0] = *(row_pointers[y] + dx + 0);
                desc.bitmap[dst + dx + 1] = *(row_pointers[y] + dx + 1);
                desc.bitmap[dst + dx + 2] = *(row_pointers[y] + dx + 2);
                desc.bitmap[dst + dx + 3] = *(row_pointers[y] + dx + 3);
            }

            updateProgress((float)y / desc.height);

            delete[] row_pointers[y];
        }
    }
    else
    {
        for(unsigned y = 0; y < desc.height; y++)
        {
            delete[] row_pointers[y];
        }
        printf("Should't be happened\n");
    }

    delete[] row_pointers;

    png_destroy_read_struct(&png, &info, NULL);

    return true;
}

// load frame in ordinary format
bool CFormatIco::loadOrdinaryFrame(sBitmapDescription& desc, cFile& file, const IcoDirentry* image)
{
    file.seek(image->offset, SEEK_SET);
    uint8_t* p = new uint8_t[image->size];
    if(image->size != file.read(p, image->size))
    {
        delete[] p;
        return false;
    }

    IcoBmpInfoHeader* imgHeader = (IcoBmpInfoHeader*)p;
    desc.width = imgHeader->width;
    desc.height = imgHeader->height / 2;	// xor mask + and mask
    desc.pitch = desc.width * 4;
    desc.bpp = 32;
    desc.bppImage = imgHeader->bits;
    desc.format = GL_RGBA;

    int pitch = calcIcoPitch(desc.bppImage, desc.width);
    if(pitch == -1)
    {
        delete[] p;
        return false;
    }

    desc.bitmap.resize(desc.pitch * desc.height);

    //	std::cout << std::endl;
    //	std::cout << "--- IcoBmpInfoHeader ---" << std::endl;
    //	std::cout << "size: " << (int)imgHeader->size << "." << std::endl;
    //	std::cout << "width: " << (int)imgHeader->width << "." << std::endl;
    //	std::cout << "height: " << (int)imgHeader->height << "." << std::endl;
    //	std::cout << "planes: " << (int)imgHeader->planes << "." << std::endl;
    //	std::cout << "bits: " << (int)imgHeader->bits << "." << std::endl;
    //	std::cout << "imagesize: " << (int)imgHeader->imagesize << "." << std::endl;


    int colors = image->colors == 0 ? (1 << desc.bppImage) : image->colors;
    uint32_t* palette = (uint32_t*)&p[imgHeader->size];
    uint8_t* xorMask = &p[imgHeader->size + colors * 4];
    uint8_t* andMask = &p[imgHeader->size + colors * 4 + desc.height * pitch];

    switch(desc.bppImage)
    {
    case 1:
        for(unsigned y = 0; y < desc.height; y++)
        {
            for(unsigned x = 0; x < desc.width; x++)
            {
                uint32_t color = palette[getBit(xorMask, y * desc.width + x, desc.width)];

                unsigned idx = (desc.height - y - 1) * desc.pitch + x * 4;

                desc.bitmap[idx + 0] = ((uint8_t*)(&color))[2];
                desc.bitmap[idx + 1] = ((uint8_t*)(&color))[1];
                desc.bitmap[idx + 2] = ((uint8_t*)(&color))[0];

                if(getBit(andMask, y * desc.width + x, desc.width))
                {
                    desc.bitmap[idx + 3] = 0;
                }
                else
                {
                    desc.bitmap[idx + 3] = 255;
                }

                updateProgress((float)desc.height * desc.width / (y * desc.width + x));
            }
        }
        break;

    case 4:
        for(unsigned y = 0; y < desc.height; y++)
        {
            for(unsigned x = 0; x < desc.width; x++)
            {
                uint32_t color = palette[getNibble(xorMask, y * desc.width + x, desc.width)];

                unsigned idx = (desc.height - y - 1) * desc.pitch + x * 4;

                desc.bitmap[idx + 0] = ((uint8_t*)(&color))[2];
                desc.bitmap[idx + 1] = ((uint8_t*)(&color))[1];
                desc.bitmap[idx + 2] = ((uint8_t*)(&color))[0];

                if(getBit(andMask, y * desc.width + x, desc.width))
                {
                    desc.bitmap[idx + 3] = 0;
                }
                else
                {
                    desc.bitmap[idx + 3] = 255;
                }

                updateProgress((float)desc.height * desc.width / (y * desc.width + x));
            }
        }
        break;

    case 8:
        for(unsigned y = 0; y < desc.height; y++)
        {
            for(unsigned x = 0; x < desc.width; x++)
            {
                uint32_t color = palette[getByte(xorMask, y * desc.width + x, desc.width)];

                unsigned idx = (desc.height - y - 1) * desc.pitch + x * 4;

                desc.bitmap[idx + 0] = ((uint8_t*)(&color))[2];
                desc.bitmap[idx + 1] = ((uint8_t*)(&color))[1];
                desc.bitmap[idx + 2] = ((uint8_t*)(&color))[0];

                if(getBit(andMask, y * desc.width + x, desc.width))
                {
                    desc.bitmap[idx + 3] = 0;
                }
                else
                {
                    desc.bitmap[idx + 3] = 255;
                }

                updateProgress((float)desc.height * desc.width / (y * desc.width + x));
            }
        }
        break;

    default:
        {
            unsigned bpp = desc.bppImage / 8;
            for(unsigned y = 0; y < desc.height; y++)
            {

                uint8_t* row = xorMask + pitch * y;

                for(unsigned x = 0; x < desc.width; x++)
                {
                    unsigned idx = (desc.height - y - 1) * desc.pitch + x * 4;

                    desc.bitmap[idx + 0] = row[2];
                    desc.bitmap[idx + 1] = row[1];
                    desc.bitmap[idx + 2] = row[0];

                    if(desc.bppImage < 32)
                    {
                        if(getBit(andMask, y * desc.width + x, desc.width))
                        {
                            desc.bitmap[idx + 3] = 0;
                        }
                        else
                        {
                            desc.bitmap[idx + 3] = 255;
                        }
                    }
                    else
                    {
                        desc.bitmap[idx + 3] = row[3];
                    }

                    row += bpp;

                    updateProgress((float)desc.height * desc.width / (y * desc.width + x));
                }
            }
        }
        break;
    }

    delete[] p;

    return true;
}

int CFormatIco::calcIcoPitch(unsigned bppImage, unsigned width)
{
    switch(bppImage)
    {
    case 1:
        if((width % 32) == 0)
            return width / 8;
        return 4 * (width / 32 + 1);

    case 4:
        if((width % 8) == 0)
            return width / 2;
        return 4 * (width / 8 + 1);

    case 8:
        if((width % 4) == 0)
            return width;
        return 4 * (width / 4 + 1);

    case 24:
        if(((width * 3) % 4) == 0)
            return width * 3;
        return 4 * (width * 3 / 4 + 1);

    case 32:
        return width * 4;

    default:
        printf("Invalid bits count: %u\n", bppImage);
        return -1; //width * (bppImage / 8);
    }
}

int CFormatIco::getBit(const uint8_t* data, int bit, unsigned width)
{
    // width per line in multiples of 32 bits
    int width32 = (width % 32 == 0 ? width / 32 : width / 32 + 1);
    int line = bit / width;
    int offset = bit % width;

    int result = (data[line * width32 * 4 + offset / 8] & (1 << (7 - (offset % 8))));

    return (result ? 1 : 0);
}

int CFormatIco::getNibble(const uint8_t* data, int nibble, unsigned width)
{
    // width per line in multiples of 32 bits
    int width32 = (width % 8 == 0 ? width / 8 : width / 8 + 1);
    int line = nibble / width;
    int offset = nibble % width;

    int result = (data[line * width32 * 4 + offset / 2] & (0x0F << (4 * (1 - offset % 2))));

    if(offset % 2 == 0)
    {
        result = result >> 4;
    }

    return result;
}

int CFormatIco::getByte(const uint8_t* data, int byte, unsigned width)
{
    // width per line in multiples of 32 bits
    int width32 = (width % 4 == 0 ? width / 4 : width / 4 + 1);
    int line = byte / width;
    int offset = byte % width;

    return data[line * width32 * 4 + offset];
}

