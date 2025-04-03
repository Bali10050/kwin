/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText:: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwin_export.h>

namespace KWin
{

class KWIN_EXPORT FileDescriptor
{
public:
    FileDescriptor() = default;
    explicit FileDescriptor(int fd);
    FileDescriptor(FileDescriptor &&);
    FileDescriptor &operator=(FileDescriptor &&);
    ~FileDescriptor();

    explicit operator bool() const;

    bool isValid() const;
    int get() const;
    int take();
    void reset();
    FileDescriptor duplicate() const;

    bool isReadable() const;
    bool isClosed() const;

    static bool isReadable(int fd);
    static bool isClosed(int fd);

private:
    int m_fd = -1;
};

inline FileDescriptor::operator bool() const
{
    return isValid();
}
}
