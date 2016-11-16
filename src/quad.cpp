/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "quad.h"

#include <cmath>
#include <cstdio>

CQuad::CQuad(unsigned tw, unsigned th, const unsigned char* data, GLenum bitmapFormat)
    : m_tw(tw)
    , m_th(th)
    , m_format(bitmapFormat)
    , m_w(tw)
    , m_h(th)
    , m_filter(true)
{
    m_quad.tex = cRenderer::createTexture();
    setData(data);

    // by deafult set whole texture size
    SetSpriteSize(tw, th);
}

CQuad::~CQuad()
{
    cRenderer::deleteTexture(m_quad.tex);
}

void CQuad::setData(const unsigned char* data)
{
    cRenderer::setData(m_quad.tex, data, m_tw, m_th, m_format);
}

void CQuad::SetColor(int r, int g, int b, int a)
{
    cRenderer::setColor(&m_quad, r, g, b, a);
}

void CQuad::SetTextureRect(float x, float y, float w, float h)
{
    m_w = w;
    m_h = h;

    const float tw = 1.0f / m_tw;
    const float th = 1.0f / m_th;

    const float tx1 = x * tw;
    const float ty1 = y * th;
    const float tx2 = (x + w) * tw;
    const float ty2 = (y + h) * th;

    m_quad.v[0].tx = tx1;
    m_quad.v[0].ty = ty1;

    m_quad.v[1].tx = tx2;
    m_quad.v[1].ty = ty1;

    m_quad.v[2].tx = tx2;
    m_quad.v[2].ty = ty2;

    m_quad.v[3].tx = tx1;
    m_quad.v[3].ty = ty2;
}

void CQuad::SetSpriteSize(float w, float h)
{
    m_w = w;
    m_h = h;

    m_quad.v[0].tx = 0.0f;
    m_quad.v[0].ty = 0.0f;

    m_quad.v[1].tx = w / m_tw;
    m_quad.v[1].ty = 0.0f;

    m_quad.v[2].tx = w / m_tw;
    m_quad.v[2].ty = h / m_th;

    m_quad.v[3].tx = 0.0f;
    m_quad.v[3].ty = h / m_th;
}

void CQuad::Render(float x, float y)
{
    RenderEx(x, y, m_w, m_h);
}

void CQuad::RenderEx(float x, float y, float w, float h, int angle)
{
    if (angle == 0)
    {
        m_quad.v[0].x = x;
        m_quad.v[0].y = y;
        m_quad.v[1].x = x + w;
        m_quad.v[1].y = y;
        m_quad.v[2].x = x + w;
        m_quad.v[2].y = y + h;
        m_quad.v[3].x = x;
        m_quad.v[3].y = y + h;
    }
    else
    {
        const float a = M_PI * angle / 180.0f;
        const float c = cosf(a);
        const float s = sinf(a);

        m_quad.v[0].x = x;
        m_quad.v[0].y = y;
        m_quad.v[1].x = x + w * c;
        m_quad.v[1].y = y + w * s;
        m_quad.v[2].x = x + w * c - h * s;
        m_quad.v[2].y = y + w * s + h * c;
        m_quad.v[3].x = x - h * s;
        m_quad.v[3].y = y + h * c;
    }

    cRenderer::render(m_quad);
}

void CQuad::useFilter(bool filter)
{
    if (m_filter != filter)
    {
        m_filter = filter;

        cRenderer::bindTexture(m_quad.tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter ? GL_LINEAR : GL_NEAREST);
    }
}
