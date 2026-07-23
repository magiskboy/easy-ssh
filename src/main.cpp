#include "MainWindow.h"
#include "Logging.h"
#include "SftpTypes.h"
#include "SshWorker.h"
#include "Tunnel.h"

#include <QApplication>
#include <QVector>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("easy-ssh"));
    QApplication::setApplicationName(QStringLiteral("easy-ssh"));
    QApplication::setApplicationDisplayName(QStringLiteral("Easy SSH"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setOrganizationDomain(QStringLiteral("github.com/magiskboy/easy-ssh"));

    initLogging();
    qCWarning(lcApp) << "Starting Easy SSH" << QApplication::applicationVersion()
                     << "log:" << logFilePath();

    qRegisterMetaType<RemoteEntry>("RemoteEntry");
    qRegisterMetaType<QVector<RemoteEntry>>("QVector<RemoteEntry>");
    qRegisterMetaType<TunnelDefinition>("TunnelDefinition");
    qRegisterMetaType<SshWorker::HostKeyPrompt>("SshWorker::HostKeyPrompt");

    MainWindow window;
    window.show();

    return app.exec();
}
