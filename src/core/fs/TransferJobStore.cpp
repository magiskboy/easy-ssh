// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "TransferJobStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace
{
QString directionKey(TransferDirection direction)
{
    return direction == TransferDirection::Upload ? QStringLiteral("upload")
                                                  : QStringLiteral("download");
}

TransferDirection directionFromKey(const QString &key)
{
    return key == QLatin1String("download") ? TransferDirection::Download
                                            : TransferDirection::Upload;
}

FsBackend backendFromInt(int value)
{
    switch (value) {
    case static_cast<int>(FsBackend::Scp):
        return FsBackend::Scp;
    case static_cast<int>(FsBackend::Sftp):
        return FsBackend::Sftp;
    default:
        return FsBackend::None;
    }
}

TransferEndReason reasonFromInt(int value)
{
    switch (value) {
    case static_cast<int>(TransferEndReason::Completed):
        return TransferEndReason::Completed;
    case static_cast<int>(TransferEndReason::Canceled):
        return TransferEndReason::Canceled;
    case static_cast<int>(TransferEndReason::HashMismatch):
        return TransferEndReason::HashMismatch;
    case static_cast<int>(TransferEndReason::Failed):
        return TransferEndReason::Failed;
    case static_cast<int>(TransferEndReason::StallTimeout):
        return TransferEndReason::StallTimeout;
    case static_cast<int>(TransferEndReason::Interrupted):
    default:
        return TransferEndReason::Interrupted;
    }
}

QJsonObject jobToJson(const TransferJob &job)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("connectionId"), job.connectionId.toString(QUuid::WithoutBraces));
    obj.insert(QStringLiteral("direction"), directionKey(job.direction));
    obj.insert(QStringLiteral("localPath"), job.localPath);
    obj.insert(QStringLiteral("remoteFinalPath"), job.remoteFinalPath);
    obj.insert(QStringLiteral("filepartPath"), job.filepartPath);
    obj.insert(QStringLiteral("bytesDone"), job.bytesDone);
    obj.insert(QStringLiteral("bytesTotal"), job.bytesTotal);
    obj.insert(QStringLiteral("sourceSize"), job.sourceSize);
    obj.insert(QStringLiteral("sourceMtimeUtcMs"), job.sourceMtimeUtcMs);
    obj.insert(QStringLiteral("sha256PrefixHex"), job.sha256PrefixHex);
    obj.insert(QStringLiteral("sha256FullHex"), job.sha256FullHex);
    obj.insert(QStringLiteral("backend"), static_cast<int>(job.backend));
    obj.insert(QStringLiteral("updatedAtMs"), job.updatedAtMs);
    obj.insert(QStringLiteral("lastReason"), static_cast<int>(job.lastReason));
    obj.insert(QStringLiteral("lastMessage"), job.lastMessage);
    return obj;
}

std::optional<TransferJob> jobFromJson(const QJsonObject &obj)
{
    TransferJob job;
    job.connectionId = QUuid(obj.value(QStringLiteral("connectionId")).toString());
    if (job.connectionId.isNull()) {
        return std::nullopt;
    }
    job.direction = directionFromKey(obj.value(QStringLiteral("direction")).toString());
    job.localPath = obj.value(QStringLiteral("localPath")).toString();
    job.remoteFinalPath = obj.value(QStringLiteral("remoteFinalPath")).toString();
    job.filepartPath = obj.value(QStringLiteral("filepartPath")).toString();
    job.bytesDone = static_cast<qint64>(obj.value(QStringLiteral("bytesDone")).toDouble());
    job.bytesTotal = static_cast<qint64>(obj.value(QStringLiteral("bytesTotal")).toDouble());
    job.sourceSize = static_cast<qint64>(obj.value(QStringLiteral("sourceSize")).toDouble());
    job.sourceMtimeUtcMs =
        static_cast<qint64>(obj.value(QStringLiteral("sourceMtimeUtcMs")).toDouble());
    job.sha256PrefixHex = obj.value(QStringLiteral("sha256PrefixHex")).toString();
    job.sha256FullHex = obj.value(QStringLiteral("sha256FullHex")).toString();
    job.backend = backendFromInt(obj.value(QStringLiteral("backend")).toInt());
    job.updatedAtMs = static_cast<qint64>(obj.value(QStringLiteral("updatedAtMs")).toDouble());
    job.lastReason = reasonFromInt(obj.value(QStringLiteral("lastReason")).toInt());
    job.lastMessage = obj.value(QStringLiteral("lastMessage")).toString();
    if (job.localPath.isEmpty() || job.remoteFinalPath.isEmpty()) {
        return std::nullopt;
    }
    return job;
}
} // namespace

QString TransferJobStore::jobKey(TransferDirection direction,
                                 const QString &localPath,
                                 const QString &remoteFinalPath)
{
    const QByteArray raw = directionKey(direction).toUtf8() + '\n' + localPath.toUtf8() + '\n' +
                           remoteFinalPath.toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(raw, QCryptographicHash::Sha1).toHex());
}

QString TransferJobStore::connectionDir(const QUuid &connectionId)
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return root + QStringLiteral("/transfer-jobs/") + connectionId.toString(QUuid::WithoutBraces);
}

std::optional<TransferJob> TransferJobStore::load(const QUuid &connectionId, const QString &key)
{
    if (connectionId.isNull() || key.isEmpty()) {
        return std::nullopt;
    }
    QFile file(connectionDir(connectionId) + QLatin1Char('/') + key + QStringLiteral(".json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return std::nullopt;
    }
    return jobFromJson(doc.object());
}

std::optional<TransferJob> TransferJobStore::loadForPaths(const QUuid &connectionId,
                                                          TransferDirection direction,
                                                          const QString &localPath,
                                                          const QString &remoteFinalPath)
{
    return load(connectionId, jobKey(direction, localPath, remoteFinalPath));
}

std::optional<TransferJob> TransferJobStore::loadLatest(const QUuid &connectionId)
{
    const QDir dir(connectionDir(connectionId));
    if (!dir.exists()) {
        return std::nullopt;
    }

    std::optional<TransferJob> best;
    const auto entries = dir.entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString &name : entries) {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject()) {
            continue;
        }
        auto job = jobFromJson(doc.object());
        if (!job) {
            continue;
        }
        if (!best || job->updatedAtMs >= best->updatedAtMs) {
            best = std::move(job);
        }
    }
    return best;
}

bool TransferJobStore::save(const TransferJob &job, QString *error)
{
    if (job.connectionId.isNull()) {
        if (error) {
            *error = QStringLiteral("Missing connection id");
        }
        return false;
    }

    const QString dirPath = connectionDir(job.connectionId);
    if (!QDir().mkpath(dirPath)) {
        if (error) {
            *error = QStringLiteral("Cannot create transfer job directory");
        }
        return false;
    }

    const QString key = jobKey(job.direction, job.localPath, job.remoteFinalPath);
    const QString path = dirPath + QLatin1Char('/') + key + QStringLiteral(".json");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    const QByteArray payload = QJsonDocument(jobToJson(job)).toJson(QJsonDocument::Compact);
    if (file.write(payload) != payload.size()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    // Download: also mirror meta next to the local .filepart for discoverability.
    if (job.direction == TransferDirection::Download && !job.filepartPath.isEmpty()) {
        const QString metaPath = transferMetaPathForFilepart(job.filepartPath);
        QSaveFile meta(metaPath);
        if (meta.open(QIODevice::WriteOnly)) {
            meta.write(payload);
            meta.commit();
        }
    }
    return true;
}

bool TransferJobStore::remove(const QUuid &connectionId, const QString &key, QString *error)
{
    if (connectionId.isNull() || key.isEmpty()) {
        return true;
    }
    const QString path =
        connectionDir(connectionId) + QLatin1Char('/') + key + QStringLiteral(".json");
    if (!QFile::exists(path)) {
        return true;
    }
    if (!QFile::remove(path)) {
        if (error) {
            *error = QStringLiteral("Cannot remove transfer job");
        }
        return false;
    }
    return true;
}

bool TransferJobStore::removeJob(const TransferJob &job, QString *error)
{
    if (job.direction == TransferDirection::Download && !job.filepartPath.isEmpty()) {
        QFile::remove(transferMetaPathForFilepart(job.filepartPath));
    }
    return remove(
        job.connectionId, jobKey(job.direction, job.localPath, job.remoteFinalPath), error);
}

void TransferJobStore::removeAllForConnection(const QUuid &connectionId)
{
    QDir dir(connectionDir(connectionId));
    if (!dir.exists()) {
        return;
    }
    const auto entries = dir.entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString &name : entries) {
        QFile::remove(dir.filePath(name));
    }
}
