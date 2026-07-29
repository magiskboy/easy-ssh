#include "SshTestHarness.h"

#include "Logging.h"

#include <QCoreApplication>
#include <QTcpSocket>
#include <QUuid>
#include <QTest>

namespace
{

Connection makeDirectConnection(const IntegrationEndpoints &env)
{
    Connection connection;
    connection.id = QUuid::createUuid();
    connection.name = QStringLiteral("it-direct");
    connection.host = env.directHost;
    connection.port = env.directPort;
    connection.username = env.directUser;
    connection.authType = AuthType::Password;
    connection.source = ConnectionSource::App;
    return connection;
}

Connection makeJumpConnection(const IntegrationEndpoints &env, bool useTargetCredentials)
{
    Connection connection;
    connection.id = QUuid::createUuid();
    connection.name = QStringLiteral("it-jump");
    connection.host = env.targetHost;
    connection.port = env.targetPort;
    connection.username = env.targetUser;
    connection.authType = AuthType::Password;
    connection.source = ConnectionSource::App;

    JumpHop hop;
    hop.host = env.bastion1Host;
    hop.port = env.bastion1Port;
    hop.username = useTargetCredentials ? env.bastion1User : env.bastion1GatewayUser;
    hop.authType = AuthType::Password;
    hop.useTargetCredentials = useTargetCredentials;
    connection.jumpHops.append(hop);
    return connection;
}

SessionCredentials makeDirectCredentials(const IntegrationEndpoints &env)
{
    SessionCredentials credentials;
    credentials.targetSecret = env.directPass;
    return credentials;
}

SessionCredentials makeJumpCredentials(const IntegrationEndpoints &env, bool useTargetCredentials)
{
    SessionCredentials credentials;
    credentials.targetSecret = env.targetPass;
    credentials.gatewaySecret =
        useTargetCredentials ? env.targetPass : env.bastion1GatewayPass;
    return credentials;
}

bool stackReachable(const IntegrationEndpoints &env)
{
    QTcpSocket probe;
    probe.connectToHost(env.directHost, env.directPort);
    return probe.waitForConnected(2000);
}

} // namespace

class IntegrationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        initLogging();
        m_env = IntegrationEndpoints::fromEnvironment();
        if (!stackReachable(m_env)) {
            QSKIP("Integration stack is not running. Start it with tests/integration/scripts/up.sh");
        }
    }

    void direct_connect_and_shell()
    {
        SshTestHarness harness;
        const Connection connection = makeDirectConnection(m_env);
        const SessionCredentials credentials = makeDirectCredentials(m_env);

        QVERIFY2(harness.openSession(connection, credentials), qPrintable(harness.lastError()));
        QVERIFY2(harness.runShellCommand(QStringLiteral("echo easy-ssh-it-ok"),
                                         QStringLiteral("easy-ssh-it-ok")),
                 qPrintable(harness.lastError()));
        harness.closeSession();
    }

    void direct_sftp_list_directory()
    {
        SshTestHarness harness;
        const Connection connection = makeDirectConnection(m_env);
        const SessionCredentials credentials = makeDirectCredentials(m_env);

        QVERIFY2(harness.openSession(connection, credentials), qPrintable(harness.lastError()));

        QVector<RemoteEntry> entries;
        QVERIFY2(harness.listDirectory(QStringLiteral("."), &entries), qPrintable(harness.lastError()));
        QVERIFY(!entries.isEmpty());
        harness.closeSession();
    }

    void proxyjump_single_hop_same_credentials()
    {
        SshTestHarness harness;
        Connection connection = makeJumpConnection(m_env, true);
        const SessionCredentials credentials = makeJumpCredentials(m_env, true);

        QVERIFY2(harness.openSession(connection, credentials), qPrintable(harness.lastError()));
        QVERIFY2(harness.runShellCommand(QStringLiteral("echo jump-ok"), QStringLiteral("jump-ok")),
                 qPrintable(harness.lastError()));
        harness.closeSession();
    }

    void proxyjump_custom_gateway_credentials()
    {
        SshTestHarness harness;
        Connection connection = makeJumpConnection(m_env, false);
        const SessionCredentials credentials = makeJumpCredentials(m_env, false);

        QVERIFY2(harness.openSession(connection, credentials), qPrintable(harness.lastError()));
        QVERIFY2(harness.runShellCommand(QStringLiteral("echo gateway-auth-ok"),
                                         QStringLiteral("gateway-auth-ok")),
                 qPrintable(harness.lastError()));
        harness.closeSession();
    }

    void proxyjump_wrong_gateway_password_fails()
    {
        SshTestHarness harness;
        Connection connection = makeJumpConnection(m_env, false);
        SessionCredentials credentials = makeJumpCredentials(m_env, false);
        credentials.gatewaySecret = QStringLiteral("wrong-password");

        QVERIFY(!harness.openSession(connection, credentials));
        QVERIFY2(harness.lastError().contains(QStringLiteral("Gateway"), Qt::CaseInsensitive) ||
                     harness.lastError().contains(QStringLiteral("Connection failed"), Qt::CaseInsensitive),
                 qPrintable(harness.lastError()));
    }

    void compression_enabled_connect()
    {
        SshTestHarness harness;
        Connection connection = makeDirectConnection(m_env);
        connection.compressionEnabled = true;
        const SessionCredentials credentials = makeDirectCredentials(m_env);

        QVERIFY2(harness.openSession(connection, credentials), qPrintable(harness.lastError()));
        harness.closeSession();
    }

    void keepalive_enabled_connect()
    {
        SshTestHarness harness;
        Connection connection = makeDirectConnection(m_env);
        connection.keepAliveIntervalSec = 5;
        connection.keepAliveCountMax = 2;
        const SessionCredentials credentials = makeDirectCredentials(m_env);

        QVERIFY2(harness.openSession(connection, credentials), qPrintable(harness.lastError()));
        QTest::qWait(6500);
        harness.closeSession();
    }

private:
    IntegrationEndpoints m_env;
};

QTEST_MAIN(IntegrationTests)
#include "integration_tests.moc"
