////////////////////////////////////////////////
//
// Simple Viewer GL edition
// Andrey A. Ugolnik
// http://www.wegroup.org
// http://www.ugolnik.info
// andrey@ugolnik.info
//
////////////////////////////////////////////////

#include "selection.h"

#include <algorithm>

const int TEX_SIZE = 16;

CSelection::CSelection() :
    m_enabled(true),
    m_imageWidth(0), m_imageHeight(0),
    m_mode(MODE_NONE),
    m_corner(CORNER_NONE)
{
}

CSelection::~CSelection()
{
}

void CSelection::Init()
{
    unsigned char* buffer = new unsigned char[TEX_SIZE * TEX_SIZE * 3];
    unsigned char* p = buffer;
    bool checker_height_odd = true;
    for(int y = 0; y < TEX_SIZE; y++)
    {
        if(y % 4 == 0)
        {
            checker_height_odd = !checker_height_odd;
        }

        bool checker_width_odd = checker_height_odd;
        for(int x = 0; x < TEX_SIZE; x++)
        {
            if(x % 4 == 0)
            {
                checker_width_odd = !checker_width_odd;
            }

            const unsigned char color = (checker_width_odd == true ? 0x20 : 0xff);
            *p++ = color;
            *p++ = color;
            *p++ = color;
        }
    }

    m_selection.reset(new CQuad(TEX_SIZE, TEX_SIZE, buffer, GL_RGB));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    delete[] buffer;
}

void CSelection::SetImageDimension(int w, int h)
{
    m_imageWidth = w;
    m_imageHeight = h;
    m_rc.Clear();
}

void CSelection::MouseButton(int x, int y, bool pressed)
{
    if(pressed == true)
    {
        m_rc.Normalize();
        bool inside = m_rc.TestPoint(x, y);

        m_mouseX = x;
        m_mouseY = y;

        if(m_mode == MODE_NONE)
        {
            if(inside == true)
            {
                m_mode = (m_corner == CORNER_CENTER ? MODE_MOVE : MODE_RESIZE);
            }
            else
            {
                m_mode = MODE_SELECT;
                clampPoint(x, y);
                m_rc.SetLeftTop(x, y);
                m_rc.Clear();
            }
        }
        else if(inside == false)
        {
            m_mode = MODE_NONE;
            m_rc.Clear();
        }
    }
    else
    {
        m_mode = MODE_NONE;
    }
}

void CSelection::MouseMove(int x, int y)
{
    updateCorner(x, y);

    if(m_mode != MODE_NONE)
    {
        int dx = x - m_mouseX;
        int dy = y - m_mouseY;

        // correct selection position
        dx = std::max<int>(dx, 0 - m_rc.x1);
        dx = std::min<int>(dx, m_imageWidth - (m_rc.x1 + m_rc.GetWidth()) - 1);
        dy = std::max<int>(dy, 0 - m_rc.y1);
        dy = std::min<int>(dy, m_imageHeight - (m_rc.y1 + m_rc.GetHeight()) - 1);

        switch(m_mode)
        {
        case MODE_NONE:   // do nothing here
            break;

        case MODE_SELECT:
            clampPoint(x, y);
            m_rc.SetRightBottom(x, y);
            break;

        case MODE_MOVE:
            m_rc.ShiftRect(dx, dy);
            break;

        case MODE_RESIZE:
            switch(m_corner)
            {
            case CORNER_NONE:   // do nothing here
            case CORNER_CENTER:
                break;

            case CORNER_LEFT:
                m_rc.x1 += dx;
                break;
            case CORNER_RIGHT:
                m_rc.x2 += dx;
                break;
            case CORNER_UP:
                m_rc.y1 += dy;
                break;
            case CORNER_DOWN:
                m_rc.y2 += dy;
                break;
            case CORNER_LEUP:
                m_rc.x1 += dx;
                m_rc.y1 += dy;
                break;
            case CORNER_RIUP:
                m_rc.x2 += dx;
                m_rc.y1 += dy;
                break;
            case CORNER_LEDN:
                m_rc.x1 += dx;
                m_rc.y2 += dy;
                break;
            case CORNER_RIDN:
                m_rc.x2 += dx;
                m_rc.y2 += dy;
                break;
            }
        }

        m_mouseX = x;
        m_mouseY = y;
    }
}

void CSelection::Render(int dx, int dy)
{
    if(m_enabled == true && m_rc.IsSet() == true)
    {
        CRect<int> rc;
        setImagePos(rc, dx, dy);

        setColor(m_corner != CORNER_UP);
        renderLine(rc.x1, rc.y1, rc.x2, rc.y1);	// top line
        setColor(m_corner != CORNER_DOWN);
        renderLine(rc.x1, rc.y2, rc.x2, rc.y2);	// bottom line
        setColor(m_corner != CORNER_LEFT);
        renderLine(rc.x1, rc.y1, rc.x1, rc.y2);	// left line
        setColor(m_corner != CORNER_RIGHT);
        renderLine(rc.x2, rc.y1, rc.x2, rc.y2);	// right line
    }
}

CRect<int> CSelection::GetRect() const
{
    return m_rc;
}

int CSelection::GetCursor() const
{
    int cursor[] = { 0, 1, 2, 2, 3, 3, 4, 5, 5, 4 };
    return cursor[m_corner];
}

void CSelection::updateCorner(int x, int y)
{
    if(m_mode != MODE_NONE)
    {
        return;
    }

    m_rc.Normalize();
    if(m_rc.TestPoint(x, y) == true)
    {
        const int delta = 10;

        CRect<int> rcLe(m_rc.x1, m_rc.y1, m_rc.x1 + delta, m_rc.y2);
        CRect<int> rcRi(m_rc.x2 - delta, m_rc.y1, m_rc.x2, m_rc.y2);
        CRect<int> rcUp(m_rc.x1, m_rc.y1, m_rc.x2, m_rc.y1 + delta);
        CRect<int> rcDn(m_rc.x1, m_rc.y2 - delta, m_rc.x2, m_rc.y2);

        if(rcLe.TestPoint(x, y) == true)
        {
            if(rcUp.TestPoint(x, y) == true)
            {
                m_corner = CORNER_LEUP;
            }
            else if(rcDn.TestPoint(x, y) == true)
            {
                m_corner = CORNER_LEDN;
            }
            else
            {
                m_corner = CORNER_LEFT;
            }
        }
        else if(rcRi.TestPoint(x, y) == true)
        {
            if(rcUp.TestPoint(x, y) == true)
            {
                m_corner = CORNER_RIUP;
            }
            else if(rcDn.TestPoint(x, y) == true)
            {
                m_corner = CORNER_RIDN;
            }
            else
            {
                m_corner = CORNER_RIGHT;
            }
        }
        else if(rcUp.TestPoint(x, y) == true)
        {
            m_corner = CORNER_UP;
        }
        else if(rcDn.TestPoint(x, y) == true)
        {
            m_corner = CORNER_DOWN;
        }
        else
        {
            m_corner = CORNER_CENTER;
        }
    }
    else
    {
        m_corner = CORNER_NONE;
    }
}

void CSelection::renderLine(int x1, int y1, int x2, int y2)
{
    int x = std::min(x1, x2);
    int y = std::min(y1, y2);
    int w = (x1 == x2 ? 1 : m_rc.GetWidth());
    int h = (y1 == y2 ? 1 : m_rc.GetHeight());

    m_selection->SetSpriteSize(w, h);
    m_selection->Render(x, y);
}

void CSelection::setImagePos(CRect<int>& rc, int dx, int dy)
{
    rc.x1 = m_rc.x1 + dx;
    rc.x2 = m_rc.x2 + dx;
    rc.y1 = m_rc.y1 + dy;
    rc.y2 = m_rc.y2 + dy;
}

void CSelection::clampPoint(int& x, int& y)
{
    x = std::max<int>(x, 0);
    x = std::min<int>(x, m_imageWidth - 1);
    y = std::max<int>(y, 0);
    y = std::min<int>(y, m_imageHeight - 1);
}

void CSelection::setColor(bool std)
{
    if(std == true)
    {
        m_selection->SetColor(200, 255, 200, 150);
    }
    else
    {
        m_selection->SetColor(255, 255, 0, 255);
    }
}

