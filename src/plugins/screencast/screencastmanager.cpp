/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "screencastmanager.h"
#include "compositor.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "opengl/gltexture.h"
#include "outputscreencastsource.h"
#include "pipewirecore.h"
#include "regionscreencastsource.h"
#include "scene/workspacescene.h"
#include "screencaststream.h"
#include "wayland/display.h"
#include "wayland/output.h"
#include "wayland_server.h"
#include "window.h"
#include "windowscreencastsource.h"
#include "workspace.h"

#include <KLocalizedString>

namespace KWin
{

ScreencastManager::ScreencastManager()
    : m_screencast(new ScreencastV1Interface(waylandServer()->display(), this))
    , m_core(new PipeWireCore)
{
    m_core->init();
    connect(m_screencast, &ScreencastV1Interface::windowScreencastRequested, this, &ScreencastManager::streamWindow);
    connect(m_screencast, &ScreencastV1Interface::outputScreencastRequested, this, &ScreencastManager::streamWaylandOutput);
    connect(m_screencast, &ScreencastV1Interface::virtualOutputScreencastRequested, this, &ScreencastManager::streamVirtualOutput);
    connect(m_screencast, &ScreencastV1Interface::regionScreencastRequested, this, &ScreencastManager::streamRegion);
}

static QRegion scaleRegion(const QRegion &_region, qreal scale)
{
    if (scale == 1.) {
        return _region;
    }

    QRegion region;
    for (auto it = _region.begin(), itEnd = _region.end(); it != itEnd; ++it) {
        region += QRect(std::floor(it->x() * scale),
                        std::floor(it->y() * scale),
                        std::ceil(it->width() * scale),
                        std::ceil(it->height() * scale));
    }

    return region;
}

class WindowStream : public ScreenCastStream
{
    Q_OBJECT

public:
    WindowStream(Window *window, std::shared_ptr<PipeWireCore> pwCore, QObject *parent)
        : ScreenCastStream(new WindowScreenCastSource(window), pwCore, parent)
        , m_window(window)
    {
        m_timer.setInterval(0);
        m_timer.setSingleShot(true);
        setObjectName(window->desktopFileName());
        connect(&m_timer, &QTimer::timeout, this, &WindowStream::bufferToStream);
        connect(this, &ScreenCastStream::startStreaming, this, &WindowStream::startFeeding);
        connect(this, &ScreenCastStream::stopStreaming, this, &WindowStream::stopFeeding);
    }

private:
    void startFeeding()
    {
        connect(m_window, &Window::damaged, this, &WindowStream::markDirty);
        markDirty();
    }

    void stopFeeding()
    {
        disconnect(m_window, &Window::damaged, this, &WindowStream::markDirty);
        m_timer.stop();
    }

    void markDirty()
    {
        m_timer.start();
    }

    void bufferToStream()
    {
        scheduleFrame(QRegion(0, 0, m_window->width(), m_window->height()));
    }

    Window *m_window;
    QTimer m_timer;
};

void ScreencastManager::streamWindow(ScreencastStreamV1Interface *waylandStream,
                                     const QString &winid,
                                     ScreencastV1Interface::CursorMode mode)
{
    auto window = Workspace::self()->findWindow(QUuid(winid));
    if (!window) {
        waylandStream->sendFailed(i18n("Could not find window id %1", winid));
        return;
    }

    auto stream = new WindowStream(window, m_core, this);
    stream->setCursorMode(mode, 1, window->clientGeometry());
    if (mode != ScreencastV1Interface::CursorMode::Hidden) {
        connect(window, &Window::clientGeometryChanged, stream, [window, stream, mode]() {
            stream->setCursorMode(mode, 1, window->clientGeometry().toRect());
        });
    }

    integrateStreams(waylandStream, stream);
}

void ScreencastManager::streamVirtualOutput(ScreencastStreamV1Interface *stream,
                                            const QString &name,
                                            const QSize &size,
                                            double scale,
                                            ScreencastV1Interface::CursorMode mode)
{
    auto output = kwinApp()->outputBackend()->createVirtualOutput(name, size, scale);
    streamOutput(stream, output, mode);
    connect(stream, &ScreencastStreamV1Interface::finished, output, [output] {
        kwinApp()->outputBackend()->removeVirtualOutput(output);
    });
}

void ScreencastManager::streamWaylandOutput(ScreencastStreamV1Interface *waylandStream,
                                            OutputInterface *output,
                                            ScreencastV1Interface::CursorMode mode)
{
    streamOutput(waylandStream, output->handle(), mode);
}

void ScreencastManager::streamOutput(ScreencastStreamV1Interface *waylandStream,
                                     Output *streamOutput,
                                     ScreencastV1Interface::CursorMode mode)
{
    if (!streamOutput) {
        waylandStream->sendFailed(i18n("Could not find output"));
        return;
    }

    auto stream = new ScreenCastStream(new OutputScreenCastSource(streamOutput), m_core, this);
    stream->setObjectName(streamOutput->name());
    stream->setCursorMode(mode, streamOutput->scale(), streamOutput->geometry());
    auto bufferToStream = [stream, streamOutput](const QRegion &damagedRegion) {
        if (!damagedRegion.isEmpty()) {
            stream->scheduleFrame(scaleRegion(damagedRegion, streamOutput->scale()));
        }
    };
    connect(streamOutput, &Output::changed, stream, [streamOutput, stream, mode]() {
        stream->setCursorMode(mode, streamOutput->scale(), streamOutput->geometry());
    });
    connect(stream, &ScreenCastStream::startStreaming, waylandStream, [streamOutput, stream, bufferToStream] {
        Compositor::self()->scene()->addRepaint(streamOutput->geometry());
        connect(streamOutput, &Output::outputChange, stream, bufferToStream);
    });
    integrateStreams(waylandStream, stream);
}

static QString rectToString(const QRect &rect)
{
    return QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

void ScreencastManager::streamRegion(ScreencastStreamV1Interface *waylandStream, const QRect &geometry, qreal scale, ScreencastV1Interface::CursorMode mode)
{
    if (!geometry.isValid()) {
        waylandStream->sendFailed(i18n("Invalid region"));
        return;
    }

    auto source = new RegionScreenCastSource(geometry, scale);
    auto stream = new ScreenCastStream(source, m_core, this);
    stream->setObjectName(rectToString(geometry));
    stream->setCursorMode(mode, scale, geometry);

    connect(stream, &ScreenCastStream::startStreaming, waylandStream, [geometry, stream, source, waylandStream] {
        Compositor::self()->scene()->addRepaint(geometry);

        bool found = false;
        const auto allOutputs = workspace()->outputs();
        for (auto output : allOutputs) {
            if (output->geometry().intersects(geometry)) {
                auto bufferToStream = [output, stream, source](const QRegion &damagedRegion) {
                    if (damagedRegion.isEmpty()) {
                        return;
                    }

                    const QRect streamRegion = source->region();
                    const QRegion region = output->pixelSize() != output->modeSize() ? output->geometry() : damagedRegion;
                    source->updateOutput(output);
                    stream->scheduleFrame(scaleRegion(region.translated(-streamRegion.topLeft()).intersected(streamRegion), source->scale()));
                };
                connect(output, &Output::outputChange, stream, bufferToStream);
                found |= true;
            }
        }
        if (!found) {
            waylandStream->sendFailed(i18n("Region outside the workspace"));
        }
    });
    integrateStreams(waylandStream, stream);
}

void ScreencastManager::integrateStreams(ScreencastStreamV1Interface *waylandStream, ScreenCastStream *stream)
{
    connect(waylandStream, &ScreencastStreamV1Interface::finished, stream, &ScreenCastStream::stop);
    connect(stream, &ScreenCastStream::stopStreaming, waylandStream, [stream, waylandStream] {
        waylandStream->sendClosed();
        stream->deleteLater();
    });
    connect(stream, &ScreenCastStream::streamReady, stream, [waylandStream](uint nodeid) {
        waylandStream->sendCreated(nodeid);
    });
    if (!stream->init()) {
        waylandStream->sendFailed(stream->error());
        delete stream;
    }
}

} // namespace KWin

#include "screencastmanager.moc"
