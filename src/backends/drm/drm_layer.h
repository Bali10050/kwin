/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputlayer.h"

#include <QRegion>
#include <memory>
#include <optional>

namespace KWin
{

class SurfaceItem;
class DrmFramebuffer;
class GLTexture;
class DrmPipeline;

class DrmOutputLayer : public OutputLayer
{
public:
    virtual ~DrmOutputLayer();

    virtual std::shared_ptr<GLTexture> texture() const;
    virtual void releaseBuffers() = 0;
};

class DrmPipelineLayer : public DrmOutputLayer
{
public:
    DrmPipelineLayer(DrmPipeline *pipeline);

    virtual bool checkTestBuffer() = 0;
    virtual std::shared_ptr<DrmFramebuffer> currentBuffer() const = 0;
    virtual OutputTransform hardwareTransform() const;
    virtual QRect bufferSourceBox() const;

protected:
    DrmPipeline *const m_pipeline;
};
}
