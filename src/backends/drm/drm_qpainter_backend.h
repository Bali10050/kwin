/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_render_backend.h"
#include "qpainter/qpainterbackend.h"

#include <QList>
#include <QObject>

namespace KWin
{

class DrmBackend;
class DrmAbstractOutput;
class DrmQPainterLayer;
class DrmPipeline;

class DrmQPainterBackend : public QPainterBackend, public DrmRenderBackend
{
    Q_OBJECT
public:
    DrmQPainterBackend(DrmBackend *backend);
    ~DrmQPainterBackend();

    DrmDevice *drmDevice() const override;

    bool present(Output *output, const std::shared_ptr<OutputFrame> &frame) override;
    void repairPresentation(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;
    OutputLayer *cursorLayer(Output *output) override;

    std::shared_ptr<DrmPipelineLayer> createDrmPlaneLayer(DrmPipeline *pipeline, DrmPlane::TypeIndex type) override;
    std::shared_ptr<DrmOutputLayer> createLayer(DrmVirtualOutput *output) override;

private:
    DrmBackend *m_backend;
};
}
