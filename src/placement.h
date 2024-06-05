/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 1997-2002 Cristian Tibirna <tibirna@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
// KWin
#include "options.h"
#include "window.h"
// Qt
#include <QList>
#include <QPoint>
#include <QRect>

class QObject;

namespace KWin
{

class VirtualDesktop;

class KWIN_EXPORT Placement : public QObject
{
    Q_OBJECT
public:
    explicit Placement();

    void place(Window *c, const QRectF &area);
    void placeSmart(Window *c, const QRectF &area, PlacementPolicy next = PlacementUnknown);

    void placeCentered(Window *c, const QRectF &area, PlacementPolicy next = PlacementUnknown);

    void reinitCascading();
    void reinitCascading(VirtualDesktop *desktop);

    void cascadeIfCovering(Window *c, const QRectF &area);

    /**
     * Cascades all clients on the current desktop
     */
    void cascadeDesktop();
    /**
     *   Unclutters the current desktop by smart-placing all clients again.
     */
    void unclutterDesktop();

    static const char *policyToString(PlacementPolicy policy);

Q_SIGNALS:
    void placing(Window *c, const QRectF &area, PlacementPolicy policy, PlacementPolicy nextPlacement);

private:
    void place(Window *c, const QRectF &area, PlacementPolicy policy, PlacementPolicy nextPlacement = PlacementUnknown);
    void placeUnderMouse(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    void placeOnMainWindow(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    void placeTransient(Window *c);

    void placeAtRandom(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    void placeCascaded(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    void placeMaximizing(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    void placeZeroCornered(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    void placeDialog(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    void placeUtility(Window *c, const QRect &area, PlacementPolicy next = PlacementUnknown);
    void placeOnScreenDisplay(Window *c, const QRect &area);

    // CT needed for cascading+
    struct DesktopCascadingInfo
    {
        QPoint pos;
        int col;
        int row;
    };

    QHash<VirtualDesktop *, DesktopCascadingInfo> cci;
};

} // namespace
