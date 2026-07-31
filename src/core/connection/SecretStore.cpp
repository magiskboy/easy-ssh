// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SecretStore.h"

#include <qt6keychain/keychain.h>

SecretStore::SecretStore(QObject *parent) : QObject(parent) {}

QString SecretStore::serviceName()
{
    return QStringLiteral("easy-ssh");
}

QString SecretStore::keyFor(const QUuid &connectionId, Kind kind)
{
    const QString id = connectionId.toString(QUuid::WithoutBraces);
    switch (kind) {
    case Kind::Passphrase:
        return id + QStringLiteral("/passphrase");
    case Kind::GatewayPassword:
        return id + QStringLiteral("/gateway/password");
    case Kind::GatewayPassphrase:
        return id + QStringLiteral("/gateway/passphrase");
    case Kind::TunnelSocksPassword:
        return id + QStringLiteral("/tunnel/socksPassword");
    case Kind::Password:
    default:
        return id + QStringLiteral("/password");
    }
}

void SecretStore::storeSecret(const QUuid &connectionId, Kind kind, const QString &value)
{
    auto *job = new QKeychain::WritePasswordJob(serviceName());
    job->setKey(keyFor(connectionId, kind));
    job->setTextData(value);

    connect(job,
            &QKeychain::WritePasswordJob::finished,
            this,
            [this, connectionId, kind](QKeychain::Job *j) {
                if (j->error() == QKeychain::NoError) {
                    emit storeFinished(connectionId, kind, true, {});
                    return;
                }
                emit storeFinished(connectionId, kind, false, j->errorString());
            });

    job->start();
}

void SecretStore::readSecret(const QUuid &connectionId, Kind kind)
{
    auto *job = new QKeychain::ReadPasswordJob(serviceName());
    job->setKey(keyFor(connectionId, kind));

    connect(job,
            &QKeychain::ReadPasswordJob::finished,
            this,
            [this, connectionId, kind](QKeychain::Job *j) {
                auto *readJob = static_cast<QKeychain::ReadPasswordJob *>(j);
                if (j->error() == QKeychain::NoError) {
                    emit readFinished(connectionId, kind, readJob->textData(), true, {});
                    return;
                }
                if (j->error() == QKeychain::EntryNotFound) {
                    emit readFinished(connectionId, kind, {}, true, {});
                    return;
                }
                emit readFinished(connectionId, kind, {}, false, j->errorString());
            });

    job->start();
}

void SecretStore::deleteSecret(const QUuid &connectionId, Kind kind)
{
    auto *job = new QKeychain::DeletePasswordJob(serviceName());
    job->setKey(keyFor(connectionId, kind));

    connect(job,
            &QKeychain::DeletePasswordJob::finished,
            this,
            [this, connectionId, kind](QKeychain::Job *j) {
                if (j->error() == QKeychain::NoError || j->error() == QKeychain::EntryNotFound) {
                    emit deleteFinished(connectionId, kind, true, {});
                    return;
                }
                emit deleteFinished(connectionId, kind, false, j->errorString());
            });

    job->start();
}

void SecretStore::deleteAllSecrets(const QUuid &connectionId)
{
    deleteSecret(connectionId, Kind::Password);
    deleteSecret(connectionId, Kind::Passphrase);
    deleteSecret(connectionId, Kind::GatewayPassword);
    deleteSecret(connectionId, Kind::GatewayPassphrase);
}

void SecretStore::copySecret(const QUuid &fromId, const QUuid &toId, Kind kind)
{
    auto *job = new QKeychain::ReadPasswordJob(serviceName());
    job->setKey(keyFor(fromId, kind));

    connect(job,
            &QKeychain::ReadPasswordJob::finished,
            this,
            [this, fromId, toId, kind](QKeychain::Job *j) {
                auto *readJob = static_cast<QKeychain::ReadPasswordJob *>(j);
                if (j->error() == QKeychain::EntryNotFound) {
                    emit storeFinished(toId, kind, true, {});
                    return;
                }
                if (j->error() != QKeychain::NoError) {
                    emit storeFinished(toId, kind, false, j->errorString());
                    return;
                }

                const QString value = readJob->textData();
                if (value.isEmpty()) {
                    emit storeFinished(toId, kind, true, {});
                    return;
                }

                storeSecret(toId, kind, value);
                Q_UNUSED(fromId);
            });

    job->start();
}
