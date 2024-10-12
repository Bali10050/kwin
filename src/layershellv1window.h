/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandwindow.h"

namespace KWin
{

class AutoHideScreenEdgeV1Interface;
class LayerSurfaceV1Interface;
class Output;
class LayerShellV1Integration;

class LayerShellV1Window : public WaylandWindow
{
    Q_OBJECT

public:
    explicit LayerShellV1Window(LayerSurfaceV1Interface *shellSurface,
                                Output *output,
                                LayerShellV1Integration *integration);

    LayerSurfaceV1Interface *shellSurface() const;
    Output *desiredOutput() const;

    WindowType windowType() const override;
    bool isPlaceable() const override;
    bool isCloseable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isResizable() const override;
    bool takeFocus() override;
    bool wantsInput() const override;
    bool dockWantsInput() const override;
    StrutRect strutRect(StrutArea area) const override;
    bool hasStrut() const override;
    void destroyWindow() override;
    void closeWindow() override;
    void setVirtualKeyboardGeometry(const BoxF &geo) override;
    void showOnScreenEdge() override;

    void installAutoHideScreenEdgeV1(AutoHideScreenEdgeV1Interface *edge);

protected:
    Layer belongsToLayer() const override;
    bool acceptsFocus() const override;
    void moveResizeInternal(const BoxF &rect, MoveResizeMode mode) override;
    void doSetPreferredBufferScale() override;
    void doSetPreferredBufferTransform() override;
    void doSetPreferredColorDescription() override;

private:
    void handleSizeChanged();
    void handleUnmapped();
    void handleCommitted();
    void handleAcceptsFocusChanged();
    void handleOutputEnabledChanged();
    void scheduleRearrange();
    void activateScreenEdge();
    void deactivateScreenEdge();
    void reserveScreenEdge();
    void unreserveScreenEdge();

    Output *m_desiredOutput;
    LayerShellV1Integration *m_integration;
    LayerSurfaceV1Interface *m_shellSurface;
    QPointer<AutoHideScreenEdgeV1Interface> m_screenEdge;
    bool m_screenEdgeActive = false;
    WindowType m_windowType;
};

} // namespace KWin
