#pragma once

#include <QObject>
#include <QUuid>

class SecretStore final : public QObject
{
    Q_OBJECT

public:
    enum class Kind
    {
        Password,
        Passphrase,
    };

    explicit SecretStore(QObject *parent = nullptr);

    void storeSecret(const QUuid &connectionId, Kind kind, const QString &value);
    void readSecret(const QUuid &connectionId, Kind kind);
    void deleteSecret(const QUuid &connectionId, Kind kind);
    void deleteAllSecrets(const QUuid &connectionId);
    void copySecret(const QUuid &fromId, const QUuid &toId, Kind kind);

signals:
    void storeFinished(const QUuid &connectionId, Kind kind, bool ok, const QString &error);
    void readFinished(
        const QUuid &connectionId, Kind kind, bool ok, const QString &value, const QString &error);
    void deleteFinished(const QUuid &connectionId, Kind kind, bool ok, const QString &error);

private:
    static QString serviceName();
    static QString keyFor(const QUuid &connectionId, Kind kind);
};
