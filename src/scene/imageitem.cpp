/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/imageitem.h"

#include "opengl/gltexture.h"

namespace KWin
{

ImageItem::ImageItem(Item *parent)
    : Item(parent)
{
}

QImage ImageItem::image() const
{
    return m_image;
}

void ImageItem::setImage(const QImage &image)
{
    m_image = image;
}

ImageItemOpenGL::ImageItemOpenGL(Item *parent)
    : ImageItem(parent)
{
}

ImageItemOpenGL::~ImageItemOpenGL()
{
}

GLTexture *ImageItemOpenGL::texture() const
{
    return m_texture.get();
}

void ImageItemOpenGL::preprocess()
{
    if (m_image.isNull()) {
        m_texture.reset();
        m_textureKey = 0;
    } else if (m_textureKey != m_image.cacheKey()) {
        m_textureKey = m_image.cacheKey();

        if (!m_texture || m_texture->size() != m_image.size()) {
            m_texture = GLTexture::upload(m_image);
            if (!m_texture) {
                return;
            }
            m_texture->setFilter(GL_LINEAR);
            m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
        } else {
            m_texture->update(m_image, m_image.rect());
        }
    }
}

WindowQuadList ImageItemOpenGL::buildQuads() const
{
    const QRectF geometry = boundingRect();
    if (geometry.isEmpty()) {
        return WindowQuadList{};
    }

    WindowQuad quad;
    quad[0] = WindowVertex(geometry.topLeft(), QPointF(0, 0));
    quad[1] = WindowVertex(geometry.topRight(), QPointF(1, 0));
    quad[2] = WindowVertex(geometry.bottomRight(), QPointF(1, 1));
    quad[3] = WindowVertex(geometry.bottomLeft(), QPointF(0, 1));

    WindowQuadList ret;
    ret.append(quad);
    return ret;
}

} // namespace KWin

#include "moc_imageitem.cpp"
