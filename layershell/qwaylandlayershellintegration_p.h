/*
 *   SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@blue-systems.com>
 *   SPDX-FileCopyrightText: 2018 Drew DeVault <sir@cmpwn.com>
 *
 *   SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef _LAYERSHELLINTEGRATION_P_H
#define _LAYERSHELLINTEGRATION_P_H

#include <wayland-client.h>

#include <QtWaylandClient/private/qwaylandshellintegration_p.h>

namespace QtLayerShell {

class QWaylandLayerShell;

class QWaylandLayerShellIntegration : public QtWaylandClient::QWaylandShellIntegration
{
public:
    QWaylandLayerShellIntegration();

    bool initialize(QtWaylandClient::QWaylandDisplay *display) override;
    QtWaylandClient::QWaylandShellSurface *createShellSurface(
            QtWaylandClient::QWaylandWindow *window) override;

private:
    static void registryLayer(void *data, struct wl_registry *registry,
            uint32_t id, const QString &interface, uint32_t version);

    QWaylandLayerShell *m_layerShell;
};

}

#endif
