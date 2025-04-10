/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KWin
{

class GLTexture;
class Shadow;
class Window;

class KWIN_EXPORT ShadowTextureProvider
{
public:
    explicit ShadowTextureProvider(Shadow *shadow);
    virtual ~ShadowTextureProvider();

    Shadow *shadow() const { return m_shadow; }

    virtual void update() = 0;

protected:
    Shadow *m_shadow;
};

class OpenGLShadowTextureProvider : public ShadowTextureProvider
{
public:
    explicit OpenGLShadowTextureProvider(Shadow *shadow);
    ~OpenGLShadowTextureProvider() override;

    GLTexture *shadowTexture()
    {
        return m_texture.get();
    }

    void update() override;

private:
    std::shared_ptr<GLTexture> m_texture;
};

class QPainterShadowTextureProvider : public ShadowTextureProvider
{
public:
    explicit QPainterShadowTextureProvider(Shadow *shadow);

    void update() override;
};

/**
 * The ShadowItem class represents a nine-tile patch server-side drop-shadow.
 */
class KWIN_EXPORT ShadowItem : public Item
{
    Q_OBJECT

public:
    explicit ShadowItem(Shadow *shadow, Window *window, Item *parent = nullptr);
    ~ShadowItem() override;

    Shadow *shadow() const;
    ShadowTextureProvider *textureProvider() const;

protected:
    WindowQuadList buildQuads() const override;
    void preprocess() override;

private Q_SLOTS:
    void handleTextureChanged();
    void updateGeometry();

private:
    Window *m_window;
    Shadow *m_shadow = nullptr;
    std::unique_ptr<ShadowTextureProvider> m_textureProvider;
    bool m_textureDirty = true;
};

} // namespace KWin
