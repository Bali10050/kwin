/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xwaylandshell_v1.h"
#include "display.h"
#include "surface.h"

#include "qwayland-server-xwayland-shell-v1.h"

#include <QPointer>

namespace KWin
{

static const quint32 s_version = 1;

class XwaylandShellV1InterfacePrivate : public QtWaylandServer::xwayland_shell_v1
{
public:
    XwaylandShellV1InterfacePrivate(Display *display, XwaylandShellV1Interface *q);

    XwaylandShellV1Interface *q;
    QList<XwaylandSurfaceV1Interface *> m_surfaces;

protected:
    void xwayland_shell_v1_destroy(Resource *resource) override;
    void xwayland_shell_v1_get_xwayland_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

class XwaylandSurfaceV1Commit : public SurfaceAttachedState<XwaylandSurfaceV1Commit>
{
public:
    std::optional<uint64_t> serial;
};

class XwaylandSurfaceV1InterfacePrivate : public SurfaceExtension<XwaylandSurfaceV1InterfacePrivate, XwaylandSurfaceV1Commit>, public QtWaylandServer::xwayland_surface_v1
{
public:
    XwaylandSurfaceV1InterfacePrivate(XwaylandShellV1Interface *shell, SurfaceInterface *surface, wl_client *client, uint32_t id, int version, XwaylandSurfaceV1Interface *q);

    void apply(XwaylandSurfaceV1Commit *commit);

    XwaylandSurfaceV1Interface *q;
    XwaylandShellV1Interface *shell;
    std::optional<uint64_t> serial;

protected:
    void xwayland_surface_v1_destroy_resource(Resource *resource) override;
    void xwayland_surface_v1_set_serial(Resource *resource, uint32_t serial_lo, uint32_t serial_hi) override;
    void xwayland_surface_v1_destroy(Resource *resource) override;
};

XwaylandShellV1InterfacePrivate::XwaylandShellV1InterfacePrivate(Display *display, XwaylandShellV1Interface *q)
    : QtWaylandServer::xwayland_shell_v1(*display, s_version)
    , q(q)
{
}

void XwaylandShellV1InterfacePrivate::xwayland_shell_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XwaylandShellV1InterfacePrivate::xwayland_shell_v1_get_xwayland_surface(Resource *resource, uint32_t id, ::wl_resource *surface_resource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);

    if (const SurfaceRole *role = surface->role()) {
        if (role != XwaylandSurfaceV1Interface::role()) {
            wl_resource_post_error(resource->handle, error_role, "the surface already has a role assigned %s", role->name().constData());
            return;
        }
    } else {
        surface->setRole(XwaylandSurfaceV1Interface::role());
    }

    auto xwaylandSurface = new XwaylandSurfaceV1Interface(q, surface, resource->client(), id, resource->version());
    m_surfaces.append(xwaylandSurface);
    QObject::connect(xwaylandSurface, &QObject::destroyed, q, [this, xwaylandSurface]() {
        m_surfaces.removeOne(xwaylandSurface);
    });
}

XwaylandSurfaceV1InterfacePrivate::XwaylandSurfaceV1InterfacePrivate(XwaylandShellV1Interface *shell, SurfaceInterface *surface, wl_client *client, uint32_t id, int version, XwaylandSurfaceV1Interface *q)
    : SurfaceExtension(surface)
    , QtWaylandServer::xwayland_surface_v1(client, id, version)
    , q(q)
    , shell(shell)
{
}

void XwaylandSurfaceV1InterfacePrivate::apply(XwaylandSurfaceV1Commit *commit)
{
    if (commit->serial.has_value()) {
        serial = commit->serial;
        Q_EMIT shell->surfaceAssociated(q);
    }
}

void XwaylandSurfaceV1InterfacePrivate::xwayland_surface_v1_destroy_resource(Resource *resource)
{
    delete q;
}

void XwaylandSurfaceV1InterfacePrivate::xwayland_surface_v1_set_serial(Resource *resource, uint32_t serial_lo, uint32_t serial_hi)
{
    const uint64_t serial = (uint64_t(serial_hi) << 32) | serial_lo;
    if (!serial) {
        wl_resource_post_error(resource->handle, error_invalid_serial, "given serial is 0");
        return;
    }

    if (this->serial.has_value()) {
        wl_resource_post_error(resource->handle, error_already_associated,
                               "xwayland_surface_v1 already has a serial assigned to it: %" PRIu64, this->serial.value());
        return;
    }

    pending->serial = serial;
}

void XwaylandSurfaceV1InterfacePrivate::xwayland_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

XwaylandShellV1Interface::XwaylandShellV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new XwaylandShellV1InterfacePrivate(display, this))
{
}

XwaylandShellV1Interface::~XwaylandShellV1Interface()
{
}

XwaylandSurfaceV1Interface *XwaylandShellV1Interface::findSurface(uint64_t serial) const
{
    for (XwaylandSurfaceV1Interface *surface : std::as_const(d->m_surfaces)) {
        if (surface->serial() == serial) {
            return surface;
        }
    }
    return nullptr;
}

XwaylandSurfaceV1Interface::XwaylandSurfaceV1Interface(XwaylandShellV1Interface *shell, SurfaceInterface *surface, wl_client *client, uint32_t id, int version)
    : d(new XwaylandSurfaceV1InterfacePrivate(shell, surface, client, id, version, this))
{
}

XwaylandSurfaceV1Interface::~XwaylandSurfaceV1Interface()
{
}

SurfaceRole *XwaylandSurfaceV1Interface::role()
{
    static SurfaceRole role(QByteArrayLiteral("xwayland_surface_v1"));
    return &role;
}

SurfaceInterface *XwaylandSurfaceV1Interface::surface() const
{
    return d->surface;
}

std::optional<uint64_t> XwaylandSurfaceV1Interface::serial() const
{
    return d->serial;
}

} // namespace KWin

#include "moc_xwaylandshell_v1.cpp"
