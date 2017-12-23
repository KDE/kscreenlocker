/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2017 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "seccomp_filter.h"
#include "kwinglplatform.h"

#include <KWindowSystem>

#include <QDBusConnection>
#include <QOpenGLContext>
#include <QOffscreenSurface>

#include <seccomp.h>
#include <sys/socket.h>
#include <fcntl.h>

namespace ScreenLocker
{
namespace SecComp
{

void init()
{
    // trigger OpenGL context creation
    // we need this to ensure that all required files are opened for write
    // on NVIDIA we need to keep write around, otherwise BUG 384005 happens
    bool writeSupported = true;
    // Mesa's software renderers create buffers in $XDG_RUNTIME_DIR on wayland
    bool createSupported = true;
    QScopedPointer<QOffscreenSurface> dummySurface(new QOffscreenSurface);
    dummySurface->create();
    QOpenGLContext dummyGlContext;
    if (dummyGlContext.create()) {
        if (dummyGlContext.makeCurrent(dummySurface.data())) {
            auto gl = KWin::GLPlatform::instance();
            gl->detect();
            gl->printResults();
            if (gl->driver() == KWin::Driver_NVidia) {
                // BUG: 384005
                writeSupported = false;
            }
            else if (gl->isSoftwareEmulation() && KWindowSystem::isPlatformWayland()) {
                createSupported = writeSupported = false;
            }
        }
    }

    // access DBus to have the socket open
    QDBusConnection::sessionBus();

    // default action: allow
    // we cannot use a whitelist approach of syscalls
    // Qt, OpenGL, DBus just need to much and too broad
    auto context = seccomp_init(SCMP_ACT_ALLOW);
    if (!context) {
        return;
    }
    // add a filter to prevent that the password gets written to a file
    // we cannot disallow write syscall. That one is needed to wake up threads
    // Qt and OpenGL might create additional threads and then it would fail as we have an fd which
    // is not allowed to write to

    // instead disallow opening new files for writing
    // they should fail with EPERM error
    if (writeSupported) {
        seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(open), 1, SCMP_A1(SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY));
        seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(openat), 1, SCMP_A2(SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY));
        seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(open), 1, SCMP_A1(SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR));
        seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(openat), 1, SCMP_A2(SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR));
    }
    if (createSupported) {
        seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(openat), 1, SCMP_A2(SCMP_CMP_MASKED_EQ, O_CREAT, O_CREAT));
        seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(open), 1, SCMP_A1(SCMP_CMP_MASKED_EQ, O_CREAT, O_CREAT));
    }

    // disallow going to a socket
    seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(socket), 0);

    // disallow fork+exec
    // note glibc seems to use clone which is allowed for threads
    seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(fork), 0);
    seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(vfork), 0);
    seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(execve), 0);
    seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(execveat), 0);

    // disallow pipe, that should destroy copy and paste on Wayland
    seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(pipe), 0);
    seccomp_rule_add(context, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(pipe2), 0);

    // and activate our rules
    seccomp_load(context);
    seccomp_release(context);
}

}
}
