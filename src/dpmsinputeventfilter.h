/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "input.h"

#include <QElapsedTimer>
#include <QObject>

#include <kwin_export.h>

namespace KWin
{

class DrmBackend;

class KWIN_EXPORT DpmsInputEventFilter : public QObject, public InputEventFilter
{
    Q_OBJECT
public:
    DpmsInputEventFilter();
    ~DpmsInputEventFilter() override;

    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override;
    bool wheelEvent(WheelEvent *event) override;
    bool keyEvent(KeyEvent *event) override;
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchUp(qint32 id, std::chrono::microseconds time) override;
    bool tabletToolEvent(TabletEvent *event) override;
    bool tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletToolId, std::chrono::microseconds time) override;
    bool tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId, std::chrono::microseconds time) override;
    bool tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time) override;
    bool tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time) override;

private:
    void notify();
    QElapsedTimer m_doubleTapTimer;
    QList<qint32> m_touchPoints;
    bool m_secondTap = false;
    bool m_enableDoubleTap;
};

}
