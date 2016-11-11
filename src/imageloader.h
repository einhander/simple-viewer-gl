/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "formats/format.h"
#include "common/bitmap_description.h"

#include <memory>

class iCallbacks;

enum class eImageType
{
#if defined(IMLIB2_SUPPORT)
    COMMON,
#endif
    JPG,
    PSD,
    PNG,
    GIF,
    ICO,
    TIF,
    XWD,
    DDS,
    RAW,
    AGE,
    PPM,
    PVR,
    SCR,

    NOTAVAILABLE,

    COUNT
};

class CImageLoader final
{
public:
    CImageLoader(iCallbacks* callbacks);
    ~CImageLoader();

    void LoadImage(const char* path);
    void LoadSubImage(unsigned subImage);
    bool isLoaded() const;

    const unsigned char* GetBitmap() const;
    void FreeMemory();
    unsigned GetWidth() const;
    unsigned GetHeight() const;
    unsigned GetPitch() const;
    unsigned GetBitmapFormat() const;
    unsigned GetBpp() const;
    unsigned GetImageBpp() const;
    long GetFileSize() const;
    size_t GetSizeMem() const;
    unsigned GetSub() const;
    unsigned GetSubCount() const;
    const char* getImageType() const;

private:
    CImageLoader();
    eImageType getType(const char* name);

private:
    CFormat* m_image = nullptr;
    std::unique_ptr<CFormat> m_formats[(unsigned)eImageType::COUNT];
    sBitmapDescription m_desc;
};
