/////////////////////////////////////////////////
//
// Andrey A. Ugolnik
// andrey@ugolnik.info
//
/////////////////////////////////////////////////

#include "pixelinfo.h"
#include "img-pointer-cross.c"

#include <sstream>
#include <iomanip>

const int BORDER	= 4;
const int ALPHA		= 200;
const int FONT_HEIGHT	= 13;
const int FRAME_DELTA	= 10;
const int LINES_COUNT	= 4;

CPixelInfo::CPixelInfo()
    : m_visible(false)
    , m_windowWidth(0)
    , m_windowHeight(0)
{
    memset(&m_pixelInfo, 0, sizeof(m_pixelInfo));
}

CPixelInfo::~CPixelInfo()
{
}

void CPixelInfo::Init()
{
    m_bg.reset(new CQuad(0, 0));
    m_bg->SetColor(0, 0, 0, ALPHA);

    int format = (imgPointerCross.bytes_per_pixel == 3 ? GL_RGB : GL_RGBA);
    m_pointer.reset(new CQuadSeries(imgPointerCross.width, imgPointerCross.height, imgPointerCross.pixel_data, format));
    m_pointer->Setup(21, 21, 10);
    SetCursor(0);

    m_ft.reset(new CFTString(FONT_HEIGHT));
    m_ft->SetColor(255, 255, 255, ALPHA);
}

void CPixelInfo::Update(const PixelInfo* _p)
{
    memcpy(&m_pixelInfo, _p, sizeof(m_pixelInfo));

    std::stringstream info;

    // TODO correct cursor position according scale factor
    if(checkBoundary() == true)
    {
        info << "pos: " << _p->img_x << " x " << _p->img_y;
        info << "\nargb: " << std::hex << std::uppercase;
        info << std::setw(2) << std::setfill('0') << _p->a;
        info << std::setw(2) << std::setfill('0') << _p->r;
        info << std::setw(2) << std::setfill('0') << _p->g;
        info << std::setw(2) << std::setfill('0') << _p->b;

        info << "\nrect: " << std::dec;

        if(_p->rc.IsSet() == true)
        {
            int x = std::min(_p->rc.x1, _p->rc.x2);
            int y = std::min(_p->rc.y1, _p->rc.y2);
            int w = _p->rc.GetWidth();
            int h = _p->rc.GetHeight();

            info << x << ", " << y << " -> " << x + w << ", " << y + h;
            info << "\nsize: " << (w + 1) << " x " << (h + 1);
        }
    }

    m_ft->Update(info.str().c_str());
}

void CPixelInfo::Render()
{
    if(m_visible == true)
    {
        m_pointer->Render(m_pixelInfo.cursor_x - 10, m_pixelInfo.cursor_y - 10);

        if(checkBoundary() == true)
        {
            int frameWidth = m_ft->GetStringWidth() + 2 * BORDER;
            int frameHeight = FONT_HEIGHT * LINES_COUNT + 2 * BORDER;

            int cursor_x = m_pixelInfo.cursor_x + FRAME_DELTA;
            int cursor_y = m_pixelInfo.cursor_y + FRAME_DELTA;
            if(cursor_x > m_windowWidth - frameWidth)
            {
                //cursorx = m_windowWidth - frameWidth;
                cursor_x = m_pixelInfo.cursor_x - FRAME_DELTA - frameWidth;
            }
            if(cursor_y > m_windowHeight - frameHeight)
            {
                //cursory = m_windowHeight - frameHeight;
                cursor_y = m_pixelInfo.cursor_y - FRAME_DELTA - frameHeight;
            }

            m_bg->SetSpriteSize(frameWidth, frameHeight);
            m_bg->Render(cursor_x, cursor_y);

            m_ft->Render(cursor_x + BORDER, cursor_y + FONT_HEIGHT);
        }
    }
}

void CPixelInfo::SetWindowSize(int w, int h)
{
    m_windowWidth = w;
    m_windowHeight = h;
}

bool CPixelInfo::checkBoundary() const
{
    if(
            m_pixelInfo.img_x >= 0
            && m_pixelInfo.img_x < m_pixelInfo.w
            && m_pixelInfo.img_y >= 0
            && m_pixelInfo.img_y < m_pixelInfo.h
      )
    {
        return true;
    }

    return false;
}

void CPixelInfo::SetCursor(int cursor)
{
    m_pointer->SetFrame(cursor);
}

