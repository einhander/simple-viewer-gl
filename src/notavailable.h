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

class CNotAvailable : public CFormat
{
public:
    CNotAvailable();
    virtual ~CNotAvailable();

    virtual bool Load(const char* filename, sBitmapDescription& desc) override;
};
