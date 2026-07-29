#include "SshTestHarness.h"

#include "Logging.h"
#include "SshWorker.h"
#include "SftpTypes.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QThread>
#include <QTimer>

namespace
{

QString envOrDefault(const char *name, const QString &fallback = {})
{
    const QByteArray value = qgetenv(name);
    return value.isEmpty() ? fallback : QString::fromUtf8(value);
}

quint16 envPortOrDefault(const char *name, quint16 fallback)
{
    bool ok = false;
    const int value = qEnvironmentVariableIntValue(name, &ok);
    if (!ok || value <= 0 || value > 65535) {
        return fallback;
    }
    return static_cast<quint16>(value);
}

void loadEnvFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const QByteArray key = line.left(eq).toUtf8();
        const QByteArray value = line.mid(eq + 1).toUtf8();
        if (qEnvironmentVariableIsEmpty(key.constData())) {
            qputenv(key.constData(), value);
        }
    }
}

} // namespace

IntegrationEndpoints IntegrationEndpoints::fromEnvironment()
{
    const QString envPath = QCoreApplication::applicationDirPath() + QStringLiteral("/../fixtures/endpoints.env");
    const QString repoEnvPath =
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../fixtures/endpoints.env");
    loadEnvFile(repoEnvPath);
    loadEnvFile(envPath);

    IntegrationEndpoints endpoints;
    endpoints.directHost = envOrDefault("EASY_SSH_IT_DIRECT_HOST", QStringLiteral("127.0.0.1"));
    endpoints.directPort = envPortOrDefault("EASY_SSH_IT_DIRECT_PORT", 12222);
    endpoints.directUser = envOrDefault("EASY_SSH_IT_DIRECT_USER", QStringLiteral("direct"));
    endpoints.directPass = envOrDefault("EASY_SSH_IT_DIRECT_PASS", QStringLiteral("passdirect"));

    endpoints.bastion1Host = envOrDefault("EASY_SSH_IT_BASTION1_HOST", QStringLiteral("127.0.0.1"));
    endpoints.bastion1Port = envPortOrDefault("EASY_SSH_IT_BASTION1_PORT", 12223);
    endpoints.bastion1User = envOrDefault("EASY_SSH_IT_BASTION1_USER", QStringLiteral("jump1"));
    endpoints.bastion1Pass = envOrDefault("EASY_SSH_IT_BASTION1_PASS", QStringLiteral("passapp"));
    endpoints.bastion1GatewayUser =
        envOrDefault("EASY_SSH_IT_BASTION1_GATEWAY_USER", QStringLiteral("gateway"));
    endpoints.bastion1GatewayPass =
        envOrDefault("EASY_SSH_IT_BASTION1_GATEWAY_PASS", QStringLiteral("passjump1"));

    endpoints.targetHost = envOrDefault("EASY_SSH_IT_TARGET_HOST", QStringLiteral("ssh-target"));
    endpoints.targetPort = envPortOrDefault("EASY_SSH_IT_TARGET_PORT", 22);
    endpoints.targetUser = envOrDefault("EASY_SSH_IT_TARGET_USER", QStringLiteral("app"));
    endpoints.targetPass = envOrDefault("EASY_SSH_IT_TARGET_PASS", QStringLiteral("passapp"));

    return endpoints;
}

SshTestHarness::SshTestHarness(QObject *parent) : QObject(parent)
{
    m_thread = new QThread(this);
    m_worker = new SshWorker();
    m_worker->moveToThread(m_thread);

    connect(m_worker,
            &SshWorker::hostKeyPrompt,
            this,
            &SshTestHarness::onHostKeyPrompt,
            Qt::QueuedConnection);
    connect(m_worker, &SshWorker::errorOccurred, this, [this](const QString &message) {
        m_lastError = message;
    });

    m_thread->start();
}

void SshTestHarness::onHostKeyPrompt(SshWorker::HostKeyPrompt reason,
                                     const QString &fingerprint,
                                     const QString &context)
{
    Q_UNUSED(reason);
    Q_UNUSED(fingerprint);
    Q_UNUSED(context);
    // Worker thread blocks in verifyKnownHost(); wake it from the test thread directly.
    m_worker->respondHostKeyTrust(true);
}

SshTestHarness::~SshTestHarness()
{
    closeSession();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(5000);
    }
}

bool SshTestHarness::openSession(const Connection &connection,
                             const SessionCredentials &credentials,
                             int timeoutMs)
{
    m_lastError.clear();

    QEventLoop loop;
    bool ok = false;

    QMetaObject::Connection connectedConn =
        connect(m_worker, &SshWorker::connected, &loop, [&]() {
            ok = true;
            loop.quit();
        });
    QMetaObject::Connection errorConn =
        connect(m_worker, &SshWorker::errorOccurred, &loop, [&](const QString &) { loop.quit(); });

    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    QMetaObject::invokeMethod(m_worker,
                              [this, connection, credentials]() {
                                  m_worker->connectToHost(connection, credentials);
                              },
                              Qt::QueuedConnection);

    loop.exec();

    disconnect(connectedConn);
    disconnect(errorConn);

    if (!ok && m_lastError.isEmpty()) {
        m_lastError = QStringLiteral("Timed out waiting for SSH connection");
    }
    return ok;
}

bool SshTestHarness::runShellCommand(const QString &command,
                                     const QString &expectedOutput,
                                     int timeoutMs)
{
    const QByteArray commandBytes = (command + QLatin1Char('\n')).toUtf8();
    const QByteArray expectedBytes = expectedOutput.toUtf8();

    QEventLoop loop;
    bool matched = false;

    QMetaObject::Connection dataConn = connect(
        m_worker, &SshWorker::dataReceived, &loop, [&](const QByteArray &chunk) {
            if (chunk.contains(expectedBytes)) {
                matched = true;
                loop.quit();
            }
        });

    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    QMetaObject::invokeMethod(
        m_worker,
        [this, commandBytes]() { m_worker->writeToChannel(commandBytes); },
        Qt::QueuedConnection);

    loop.exec();
    disconnect(dataConn);

    if (!matched) {
        m_lastError = QStringLiteral("Timed out waiting for shell output containing \"%1\"")
                          .arg(expectedOutput);
    }
    return matched;
}

bool SshTestHarness::listDirectory(const QString &path,
                                   QVector<RemoteEntry> *entries,
                                   int timeoutMs)
{
    if (entries == nullptr) {
        return false;
    }

    entries->clear();
    QEventLoop loop;
    bool ok = false;

    QMetaObject::Connection listedConn = connect(
        m_worker, &SshWorker::directoryListed, &loop, [&](const QString &, const QVector<RemoteEntry> &rows) {
            *entries = rows;
            ok = true;
            loop.quit();
        });
    QMetaObject::Connection errorConn =
        connect(m_worker, &SshWorker::sftpError, &loop, [&](const QString &message) {
            m_lastError = message;
            loop.quit();
        });

    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    QMetaObject::invokeMethod(
        m_worker, [this, path]() { m_worker->listDirectory(path); }, Qt::QueuedConnection);

    loop.exec();

    disconnect(listedConn);
    disconnect(errorConn);

    if (!ok && m_lastError.isEmpty()) {
        m_lastError = QStringLiteral("Timed out waiting for SFTP directory listing");
    }
    return ok;
}

void SshTestHarness::closeSession()
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(m_worker, &SshWorker::disconnectSession, Qt::BlockingQueuedConnection);
}
