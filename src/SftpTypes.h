#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct RemoteEntry {
    QString name;
    QString path;
    bool isDir = false;
    qint64 size = 0;
    QString permissions;
    /// Unix mtime seconds; 0 if unknown.
    qint64 mtime = 0;
};

Q_DECLARE_METATYPE(RemoteEntry)
Q_DECLARE_METATYPE(QVector<RemoteEntry>)
