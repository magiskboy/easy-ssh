#pragma once

#include "Connection.h"
#include "SftpTypes.h"
#include "SshWorker.h"

#include <QObject>
#include <QString>
#include <QVector>

struct IntegrationEndpoints
{
    QString directHost;
    quint16 directPort = 2222;
    QString directUser;
    QString directPass;

    QString bastion1Host;
    quint16 bastion1Port = 2223;
    QString bastion1User;
    QString bastion1Pass;
    QString bastion1GatewayUser;
    QString bastion1GatewayPass;

    QString targetHost;
    quint16 targetPort = 22;
    QString targetUser;
    QString targetPass;

    static IntegrationEndpoints fromEnvironment();
};

class SshTestHarness final : public QObject
{
    Q_OBJECT

public:
    explicit SshTestHarness(QObject *parent = nullptr);
    ~SshTestHarness() override;

    bool openSession(const Connection &connection,
                     const SessionCredentials &credentials,
                     int timeoutMs = 15000);
    bool runShellCommand(const QString &command,
                         const QString &expectedOutput,
                         int timeoutMs = 5000);
    bool listDirectory(const QString &path, QVector<RemoteEntry> *entries, int timeoutMs = 5000);
    void closeSession();

    QString lastError() const { return m_lastError; }

private slots:
    void onHostKeyPrompt(SshWorker::HostKeyPrompt reason, const QString &fingerprint, const QString &context);

private:
    class QThread *m_thread = nullptr;
    class SshWorker *m_worker = nullptr;
    QString m_lastError;
};
