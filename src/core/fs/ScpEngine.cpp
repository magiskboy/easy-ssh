// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ScpEngine.h"

#include <QCoreApplication>

namespace
{
QString trScp(const char *text)
{
    return QCoreApplication::translate("ScpEngine", text);
}
} // namespace

bool ScpEngine::notSupported(QString *error)
{
    if (error) {
        *error = trScp("SCP is not implemented yet");
    }
    return false;
}

bool ScpEngine::open(ssh_session session, QString *failureMessage)
{
    Q_UNUSED(session);
    if (failureMessage) {
        *failureMessage = trScp("SCP is not implemented yet");
    }
    return false;
}

void ScpEngine::close() {}

bool ScpEngine::listDirectoryEntries(const QString &, QVector<RemoteEntry> *, QString *error)
{
    return notSupported(error);
}

bool ScpEngine::createDirectory(const QString &, QString *error)
{
    return notSupported(error);
}

bool ScpEngine::renamePath(const QString &, const QString &, QString *error)
{
    return notSupported(error);
}

bool ScpEngine::removeFile(const QString &, QString *error)
{
    return notSupported(error);
}

bool ScpEngine::removeDirectory(const QString &, QString *error)
{
    return notSupported(error);
}

bool ScpEngine::canonicalizePath(const QString &, QString &, QString *error)
{
    return notSupported(error);
}

bool ScpEngine::isRemoteDirectory(const QString &, bool *, QString *error)
{
    return notSupported(error);
}

bool ScpEngine::remoteFileSize(const QString &, qint64 *, QString *error)
{
    return notSupported(error);
}

bool ScpEngine::uploadFile(
    const QString &, const CancelCheck &, const QString &, const ProgressNote &, QString *error)
{
    return notSupported(error);
}

bool ScpEngine::downloadFile(
    const QString &, const CancelCheck &, const QString &, const ProgressNote &, QString *error)
{
    return notSupported(error);
}
