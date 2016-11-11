/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "formatage.h"
#include "AGEheader.h"
#include "ZlibDecoder.h"
#include "file.h"
#include "helpers.h"
#include "rle.h"

#include <cstring>

static bool isValidFormat(const AGE::Header& header, unsigned file_size)
{
    if(header.data_size + sizeof(AGE::Header) == file_size)
    {
        return AGE::isRawHeader(header);
    }
    return false;
}



cFormatAge::cFormatAge(const char* lib, const char* name, iCallbacks* callbacks)
    : CFormat(lib, name, callbacks)
{
}

cFormatAge::~cFormatAge()
{
}

bool cFormatAge::isSupported(cFile& file, Buffer& buffer) const
{
    if(!readBuffer(file, buffer, sizeof(AGE::Header)))
    {
        return false;
    }

    auto header = (const AGE::Header*)&buffer[0];
    return isValidFormat(*header, file.getSize());
}

//bool cFormatAge::isRawFormat(const char* name)
//{
    //cFile file;
    //if(!file.open(name))
    //{
        //return false;
    //}

    //Header header;
    //if(sizeof(header) != file.read(&header, sizeof(header)))
    //{
        //return false;
    //}

    //return isValidFormat(header, file.getSize());
//}

bool cFormatAge::Load(const char* filename, unsigned /*subImage*/)
{
    cFile file;
    if(!file.open(filename))
    {
        return false;
    }

    m_size = file.getSize();

    AGE::Header header;
    if(sizeof(header) != file.read(&header, sizeof(header)))
    {
        printf("not valid AGE image format\n");
        return false;
    }

    if(!isValidFormat(header, m_size))
    {
        return false;
    }

    unsigned bytespp = 0;
    switch(header.format)
    {
    case AGE::Format::ALPHA:
        bytespp = 1;
        m_format = GL_ALPHA;
        break;
    case AGE::Format::RGB:
        bytespp = 3;
        m_format = GL_RGB;
        break;
    case AGE::Format::RGBA:
        bytespp = 4;
        m_format = GL_RGBA;
        break;
    default:
        printf("unknown AGE format\n");
        return false;
    }

    m_bpp = m_bppImage = bytespp * 8;
    m_width = header.w;
    m_height = header.h;
    m_pitch = m_width * bytespp;
    m_bitmap.resize(m_pitch * m_height);

    m_info = "AGE image format";

    if(header.compression != AGE::Compression::NONE)
    {
        std::vector<unsigned char> in(header.data_size);
        if(header.data_size != file.read(&in[0], header.data_size))
        {
            return false;
        }

        progress(50);

        unsigned decoded = 0;

        if(header.compression == AGE::Compression::ZLIB)
        {
            cZlibDecoder decoder;
            decoded = decoder.decode(&in[0], in.size(), &m_bitmap[0], m_bitmap.size());
            if(!decoded)
            {
                printf("error decode ZLIB\n");
                return false;
            }
        }
        else
        {
            cRLE decoder;
            if(header.compression == AGE::Compression::RLE4)
            {
                decoded = decoder.decodeBy4((unsigned*)&in[0], in.size() / 4, (unsigned*)&m_bitmap[0], m_bitmap.size() / 4);
            }
            else
            {
                decoded = decoder.decode(&in[0], in.size(), &m_bitmap[0], m_bitmap.size());
            }
            if(!decoded)
            {
                printf("error decode RLE\n");
                return false;
            }
        }

        progress(100);
    }
    else
    {
        for(unsigned y = 0; y < m_height; y++)
        {
            if(m_pitch != file.read(&m_bitmap[y * m_pitch], m_pitch))
            {
                return false;
            }
            int percent = (int)(100.0f * y / m_height);
            progress(percent);
        }
    }

    return true;
}

