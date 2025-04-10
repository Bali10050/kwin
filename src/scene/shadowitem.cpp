/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/shadowitem.h"
#include "compositor.h"
#include "core/renderbackend.h"
#include "opengl/eglcontext.h"
#include "opengl/gltexture.h"
#include "scene/workspacescene.h"
#include "shadow.h"
#include "window.h"

#include <QPainter>

namespace KWin
{

ShadowTextureProvider::ShadowTextureProvider(Shadow *shadow)
    : m_shadow(shadow)
{
}

ShadowTextureProvider::~ShadowTextureProvider()
{
}

ShadowItem::ShadowItem(Shadow *shadow, Window *window, Item *parent)
    : Item(parent)
    , m_window(window)
    , m_shadow(shadow)
{
    switch (Compositor::self()->backend()->compositingType()) {
    case OpenGLCompositing:
        m_textureProvider = std::make_unique<OpenGLShadowTextureProvider>(shadow);
        break;
    case QPainterCompositing:
        m_textureProvider = std::make_unique<QPainterShadowTextureProvider>(shadow);
        break;
    default:
        Q_UNREACHABLE();
    }

    connect(shadow, &Shadow::offsetChanged, this, &ShadowItem::updateGeometry);
    connect(shadow, &Shadow::rectChanged, this, &ShadowItem::updateGeometry);
    connect(shadow, &Shadow::textureChanged, this, &ShadowItem::handleTextureChanged);

    updateGeometry();
    handleTextureChanged();
}

ShadowItem::~ShadowItem()
{
}

Shadow *ShadowItem::shadow() const
{
    return m_shadow;
}

ShadowTextureProvider *ShadowItem::textureProvider() const
{
    return m_textureProvider.get();
}

void ShadowItem::updateGeometry()
{
    const QRectF rect = m_shadow->rect() + m_shadow->offset();

    setPosition(rect.topLeft());
    setSize(rect.size());
    discardQuads();
}

void ShadowItem::handleTextureChanged()
{
    scheduleRepaint(rect());
    discardQuads();
    m_textureDirty = true;
}

static inline void distributeHorizontally(QRectF &leftRect, QRectF &rightRect)
{
    if (leftRect.right() > rightRect.left()) {
        const qreal boundedRight = std::min(leftRect.right(), rightRect.right());
        const qreal boundedLeft = std::max(leftRect.left(), rightRect.left());
        const qreal halfOverlap = (boundedRight - boundedLeft) / 2.0;
        leftRect.setRight(boundedRight - halfOverlap);
        rightRect.setLeft(boundedLeft + halfOverlap);
    }
}

static inline void distributeVertically(QRectF &topRect, QRectF &bottomRect)
{
    if (topRect.bottom() > bottomRect.top()) {
        const qreal boundedBottom = std::min(topRect.bottom(), bottomRect.bottom());
        const qreal boundedTop = std::max(topRect.top(), bottomRect.top());
        const qreal halfOverlap = (boundedBottom - boundedTop) / 2.0;
        topRect.setBottom(boundedBottom - halfOverlap);
        bottomRect.setTop(boundedTop + halfOverlap);
    }
}

WindowQuadList ShadowItem::buildQuads() const
{
    // Do not draw shadows if window width or window height is less than 5 px. 5 is an arbitrary choice.
    if (!m_window->wantsShadowToBeRendered() || m_window->width() < 5 || m_window->height() < 5) {
        return WindowQuadList();
    }

    const QSizeF top(m_shadow->elementSize(Shadow::ShadowElementTop));
    const QSizeF topRight(m_shadow->elementSize(Shadow::ShadowElementTopRight));
    const QSizeF right(m_shadow->elementSize(Shadow::ShadowElementRight));
    const QSizeF bottomRight(m_shadow->elementSize(Shadow::ShadowElementBottomRight));
    const QSizeF bottom(m_shadow->elementSize(Shadow::ShadowElementBottom));
    const QSizeF bottomLeft(m_shadow->elementSize(Shadow::ShadowElementBottomLeft));
    const QSizeF left(m_shadow->elementSize(Shadow::ShadowElementLeft));
    const QSizeF topLeft(m_shadow->elementSize(Shadow::ShadowElementTopLeft));

    const QMarginsF shadowMargins(
        std::max({topLeft.width(), left.width(), bottomLeft.width()}),
        std::max({topLeft.height(), top.height(), topRight.height()}),
        std::max({topRight.width(), right.width(), bottomRight.width()}),
        std::max({bottomRight.height(), bottom.height(), bottomLeft.height()}));

    const QRectF outerRect = rect();

    const int width = shadowMargins.left() + std::max(top.width(), bottom.width()) + shadowMargins.right();
    const int height = shadowMargins.top() + std::max(left.height(), right.height()) + shadowMargins.bottom();

    QRectF topLeftRect;
    if (!topLeft.isEmpty()) {
        topLeftRect = QRectF(outerRect.topLeft(), topLeft);
    } else {
        topLeftRect = QRectF(outerRect.left() + shadowMargins.left(),
                             outerRect.top() + shadowMargins.top(), 0, 0);
    }

    QRectF topRightRect;
    if (!topRight.isEmpty()) {
        topRightRect = QRectF(outerRect.right() - topRight.width(), outerRect.top(),
                              topRight.width(), topRight.height());
    } else {
        topRightRect = QRectF(outerRect.right() - shadowMargins.right(),
                              outerRect.top() + shadowMargins.top(), 0, 0);
    }

    QRectF bottomRightRect;
    if (!bottomRight.isEmpty()) {
        bottomRightRect = QRectF(outerRect.right() - bottomRight.width(),
                                 outerRect.bottom() - bottomRight.height(),
                                 bottomRight.width(), bottomRight.height());
    } else {
        bottomRightRect = QRectF(outerRect.right() - shadowMargins.right(),
                                 outerRect.bottom() - shadowMargins.bottom(), 0, 0);
    }

    QRectF bottomLeftRect;
    if (!bottomLeft.isEmpty()) {
        bottomLeftRect = QRectF(outerRect.left(), outerRect.bottom() - bottomLeft.height(),
                                bottomLeft.width(), bottomLeft.height());
    } else {
        bottomLeftRect = QRectF(outerRect.left() + shadowMargins.left(),
                                outerRect.bottom() - shadowMargins.bottom(), 0, 0);
    }

    // Re-distribute the corner tiles so no one of them is overlapping with others.
    // By doing this, we assume that shadow's corner tiles are symmetric
    // and it is OK to not draw top/right/bottom/left tile between corners.
    // For example, let's say top-left and top-right tiles are overlapping.
    // In that case, the right side of the top-left tile will be shifted to left,
    // the left side of the top-right tile will shifted to right, and the top
    // tile won't be rendered.
    distributeHorizontally(topLeftRect, topRightRect);
    distributeHorizontally(bottomLeftRect, bottomRightRect);
    distributeVertically(topLeftRect, bottomLeftRect);
    distributeVertically(topRightRect, bottomRightRect);

    qreal tx1 = 0.0,
          tx2 = 0.0,
          ty1 = 0.0,
          ty2 = 0.0;

    WindowQuadList quads;
    quads.reserve(8);

    if (topLeftRect.isValid()) {
        tx1 = 0.0;
        ty1 = 0.0;
        tx2 = topLeftRect.width();
        ty2 = topLeftRect.height();
        WindowQuad topLeftQuad;
        topLeftQuad[0] = WindowVertex(topLeftRect.left(), topLeftRect.top(), tx1, ty1);
        topLeftQuad[1] = WindowVertex(topLeftRect.right(), topLeftRect.top(), tx2, ty1);
        topLeftQuad[2] = WindowVertex(topLeftRect.right(), topLeftRect.bottom(), tx2, ty2);
        topLeftQuad[3] = WindowVertex(topLeftRect.left(), topLeftRect.bottom(), tx1, ty2);
        quads.append(topLeftQuad);
    }

    if (topRightRect.isValid()) {
        tx1 = width - topRightRect.width();
        ty1 = 0.0;
        tx2 = width;
        ty2 = topRightRect.height();
        WindowQuad topRightQuad;
        topRightQuad[0] = WindowVertex(topRightRect.left(), topRightRect.top(), tx1, ty1);
        topRightQuad[1] = WindowVertex(topRightRect.right(), topRightRect.top(), tx2, ty1);
        topRightQuad[2] = WindowVertex(topRightRect.right(), topRightRect.bottom(), tx2, ty2);
        topRightQuad[3] = WindowVertex(topRightRect.left(), topRightRect.bottom(), tx1, ty2);
        quads.append(topRightQuad);
    }

    if (bottomRightRect.isValid()) {
        tx1 = width - bottomRightRect.width();
        tx2 = width;
        ty1 = height - bottomRightRect.height();
        ty2 = height;
        WindowQuad bottomRightQuad;
        bottomRightQuad[0] = WindowVertex(bottomRightRect.left(), bottomRightRect.top(), tx1, ty1);
        bottomRightQuad[1] = WindowVertex(bottomRightRect.right(), bottomRightRect.top(), tx2, ty1);
        bottomRightQuad[2] = WindowVertex(bottomRightRect.right(), bottomRightRect.bottom(), tx2, ty2);
        bottomRightQuad[3] = WindowVertex(bottomRightRect.left(), bottomRightRect.bottom(), tx1, ty2);
        quads.append(bottomRightQuad);
    }

    if (bottomLeftRect.isValid()) {
        tx1 = 0.0;
        tx2 = bottomLeftRect.width();
        ty1 = height - bottomLeftRect.height();
        ty2 = height;
        WindowQuad bottomLeftQuad;
        bottomLeftQuad[0] = WindowVertex(bottomLeftRect.left(), bottomLeftRect.top(), tx1, ty1);
        bottomLeftQuad[1] = WindowVertex(bottomLeftRect.right(), bottomLeftRect.top(), tx2, ty1);
        bottomLeftQuad[2] = WindowVertex(bottomLeftRect.right(), bottomLeftRect.bottom(), tx2, ty2);
        bottomLeftQuad[3] = WindowVertex(bottomLeftRect.left(), bottomLeftRect.bottom(), tx1, ty2);
        quads.append(bottomLeftQuad);
    }

    QRectF topRect(QPointF(topLeftRect.right(), outerRect.top()),
                   QPointF(topRightRect.left(), outerRect.top() + top.height()));

    QRectF rightRect(QPointF(outerRect.right() - right.width(), topRightRect.bottom()),
                     QPointF(outerRect.right(), bottomRightRect.top()));

    QRectF bottomRect(QPointF(bottomLeftRect.right(), outerRect.bottom() - bottom.height()),
                      QPointF(bottomRightRect.left(), outerRect.bottom()));

    QRectF leftRect(QPointF(outerRect.left(), topLeftRect.bottom()),
                    QPointF(outerRect.left() + left.width(), bottomLeftRect.top()));

    // Re-distribute left/right and top/bottom shadow tiles so they don't
    // overlap when the window is too small. Please notice that we don't
    // fix overlaps between left/top(left/bottom, right/top, and so on)
    // corner tiles because corresponding counter parts won't be valid when
    // the window is too small, which means they won't be rendered.
    distributeHorizontally(leftRect, rightRect);
    distributeVertically(topRect, bottomRect);

    if (topRect.isValid()) {
        tx1 = shadowMargins.left();
        ty1 = 0.0;
        tx2 = tx1 + top.width();
        ty2 = topRect.height();
        WindowQuad topQuad;
        topQuad[0] = WindowVertex(topRect.left(), topRect.top(), tx1, ty1);
        topQuad[1] = WindowVertex(topRect.right(), topRect.top(), tx2, ty1);
        topQuad[2] = WindowVertex(topRect.right(), topRect.bottom(), tx2, ty2);
        topQuad[3] = WindowVertex(topRect.left(), topRect.bottom(), tx1, ty2);
        quads.append(topQuad);
    }

    if (rightRect.isValid()) {
        tx1 = width - rightRect.width();
        ty1 = shadowMargins.top();
        tx2 = width;
        ty2 = ty1 + right.height();
        WindowQuad rightQuad;
        rightQuad[0] = WindowVertex(rightRect.left(), rightRect.top(), tx1, ty1);
        rightQuad[1] = WindowVertex(rightRect.right(), rightRect.top(), tx2, ty1);
        rightQuad[2] = WindowVertex(rightRect.right(), rightRect.bottom(), tx2, ty2);
        rightQuad[3] = WindowVertex(rightRect.left(), rightRect.bottom(), tx1, ty2);
        quads.append(rightQuad);
    }

    if (bottomRect.isValid()) {
        tx1 = shadowMargins.left();
        ty1 = height - bottomRect.height();
        tx2 = tx1 + bottom.width();
        ty2 = height;
        WindowQuad bottomQuad;
        bottomQuad[0] = WindowVertex(bottomRect.left(), bottomRect.top(), tx1, ty1);
        bottomQuad[1] = WindowVertex(bottomRect.right(), bottomRect.top(), tx2, ty1);
        bottomQuad[2] = WindowVertex(bottomRect.right(), bottomRect.bottom(), tx2, ty2);
        bottomQuad[3] = WindowVertex(bottomRect.left(), bottomRect.bottom(), tx1, ty2);
        quads.append(bottomQuad);
    }

    if (leftRect.isValid()) {
        tx1 = 0.0;
        ty1 = shadowMargins.top();
        tx2 = leftRect.width();
        ty2 = ty1 + left.height();
        WindowQuad leftQuad;
        leftQuad[0] = WindowVertex(leftRect.left(), leftRect.top(), tx1, ty1);
        leftQuad[1] = WindowVertex(leftRect.right(), leftRect.top(), tx2, ty1);
        leftQuad[2] = WindowVertex(leftRect.right(), leftRect.bottom(), tx2, ty2);
        leftQuad[3] = WindowVertex(leftRect.left(), leftRect.bottom(), tx1, ty2);
        quads.append(leftQuad);
    }

    return quads;
}

void ShadowItem::preprocess()
{
    if (m_textureDirty) {
        m_textureDirty = false;
        m_textureProvider->update();
    }
}

class DecorationShadowTextureCache
{
public:
    ~DecorationShadowTextureCache();
    DecorationShadowTextureCache(const DecorationShadowTextureCache &) = delete;
    static DecorationShadowTextureCache &instance();

    void unregister(ShadowTextureProvider *provider);
    std::shared_ptr<GLTexture> getTexture(ShadowTextureProvider *provider);

private:
    DecorationShadowTextureCache() = default;
    struct Data
    {
        std::shared_ptr<GLTexture> texture;
        QList<ShadowTextureProvider *> providers;
    };
    QHash<KDecoration3::DecorationShadow *, Data> m_cache;
};

DecorationShadowTextureCache &DecorationShadowTextureCache::instance()
{
    static DecorationShadowTextureCache s_instance;
    return s_instance;
}

DecorationShadowTextureCache::~DecorationShadowTextureCache()
{
    Q_ASSERT(m_cache.isEmpty());
}

void DecorationShadowTextureCache::unregister(ShadowTextureProvider *provider)
{
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        auto &d = it.value();
        // check whether the Vector of Shadows contains our shadow and remove all of them
        auto glIt = d.providers.begin();
        while (glIt != d.providers.end()) {
            if (*glIt == provider) {
                glIt = d.providers.erase(glIt);
            } else {
                glIt++;
            }
        }
        // if there are no shadows any more we can erase the cache entry
        if (d.providers.isEmpty()) {
            it = m_cache.erase(it);
        } else {
            it++;
        }
    }
}

std::shared_ptr<GLTexture> DecorationShadowTextureCache::getTexture(ShadowTextureProvider *provider)
{
    Shadow *shadow = provider->shadow();
    Q_ASSERT(shadow->hasDecorationShadow());
    unregister(provider);
    const auto decoShadow = shadow->decorationShadow().lock();
    Q_ASSERT(decoShadow);
    auto it = m_cache.find(decoShadow.get());
    if (it != m_cache.end()) {
        Q_ASSERT(!it.value().providers.contains(provider));
        it.value().providers << provider;
        return it.value().texture;
    }
    Data d;
    d.providers << provider;
    d.texture = GLTexture::upload(shadow->decorationShadowImage());
    if (!d.texture) {
        return nullptr;
    }
    d.texture->setFilter(GL_LINEAR);
    d.texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_cache.insert(decoShadow.get(), d);
    return d.texture;
}

OpenGLShadowTextureProvider::OpenGLShadowTextureProvider(Shadow *shadow)
    : ShadowTextureProvider(shadow)
{
}

OpenGLShadowTextureProvider::~OpenGLShadowTextureProvider()
{
    if (m_texture) {
        Compositor::self()->scene()->openglContext()->makeCurrent();
        DecorationShadowTextureCache::instance().unregister(this);
        m_texture.reset();
    }
}

void OpenGLShadowTextureProvider::update()
{
    if (m_shadow->hasDecorationShadow()) {
        // simplifies a lot by going directly to
        m_texture = DecorationShadowTextureCache::instance().getTexture(this);
        return;
    }

    const QSize top(m_shadow->shadowElement(Shadow::ShadowElementTop).size());
    const QSize topRight(m_shadow->shadowElement(Shadow::ShadowElementTopRight).size());
    const QSize right(m_shadow->shadowElement(Shadow::ShadowElementRight).size());
    const QSize bottom(m_shadow->shadowElement(Shadow::ShadowElementBottom).size());
    const QSize bottomLeft(m_shadow->shadowElement(Shadow::ShadowElementBottomLeft).size());
    const QSize left(m_shadow->shadowElement(Shadow::ShadowElementLeft).size());
    const QSize topLeft(m_shadow->shadowElement(Shadow::ShadowElementTopLeft).size());
    const QSize bottomRight(m_shadow->shadowElement(Shadow::ShadowElementBottomRight).size());

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()}) + std::max(top.width(), bottom.width()) + std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()}) + std::max(left.height(), right.height()) + std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    if (width == 0 || height == 0) {
        return;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    const int innerRectTop = std::max({topLeft.height(), top.height(), topRight.height()});
    const int innerRectLeft = std::max({topLeft.width(), left.width(), bottomLeft.width()});

    QPainter p;
    p.begin(&image);

    p.drawImage(QRectF(0, 0, topLeft.width(), topLeft.height()), m_shadow->shadowElement(Shadow::ShadowElementTopLeft));
    p.drawImage(QRectF(innerRectLeft, 0, top.width(), top.height()), m_shadow->shadowElement(Shadow::ShadowElementTop));
    p.drawImage(QRectF(width - topRight.width(), 0, topRight.width(), topRight.height()), m_shadow->shadowElement(Shadow::ShadowElementTopRight));

    p.drawImage(QRectF(0, innerRectTop, left.width(), left.height()), m_shadow->shadowElement(Shadow::ShadowElementLeft));
    p.drawImage(QRectF(width - right.width(), innerRectTop, right.width(), right.height()), m_shadow->shadowElement(Shadow::ShadowElementRight));

    p.drawImage(QRectF(0, height - bottomLeft.height(), bottomLeft.width(), bottomLeft.height()), m_shadow->shadowElement(Shadow::ShadowElementBottomLeft));
    p.drawImage(QRectF(innerRectLeft, height - bottom.height(), bottom.width(), bottom.height()), m_shadow->shadowElement(Shadow::ShadowElementBottom));
    p.drawImage(QRectF(width - bottomRight.width(), height - bottomRight.height(), bottomRight.width(), bottomRight.height()), m_shadow->shadowElement(Shadow::ShadowElementBottomRight));

    p.end();

    // Check if the image is alpha-only in practice, and if so convert it to an 8-bpp format
    const auto context = EglContext::currentContext();
    if (!context->isOpenGLES() && context->supportsTextureSwizzle() && context->supportsRGTextures()) {
        QImage alphaImage(image.size(), QImage::Format_Alpha8);
        bool alphaOnly = true;

        for (ptrdiff_t y = 0; alphaOnly && y < image.height(); y++) {
            const uint32_t *const src = reinterpret_cast<const uint32_t *>(image.scanLine(y));
            uint8_t *const dst = reinterpret_cast<uint8_t *>(alphaImage.scanLine(y));

            for (ptrdiff_t x = 0; x < image.width(); x++) {
                if (src[x] & 0x00ffffff) {
                    alphaOnly = false;
                }

                dst[x] = qAlpha(src[x]);
            }
        }

        if (alphaOnly) {
            image = alphaImage;
        }
    }

    m_texture = GLTexture::upload(image);
    if (!m_texture) {
        return;
    }
    m_texture->setFilter(GL_LINEAR);
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);

    if (m_texture->internalFormat() == GL_R8) {
        // Swizzle red to alpha and all other channels to zero
        m_texture->bind();
        m_texture->setSwizzle(GL_ZERO, GL_ZERO, GL_ZERO, GL_RED);
    }
}

QPainterShadowTextureProvider::QPainterShadowTextureProvider(Shadow *shadow)
    : ShadowTextureProvider(shadow)
{
}

void QPainterShadowTextureProvider::update()
{
}

} // namespace KWin

#include "moc_shadowitem.cpp"
