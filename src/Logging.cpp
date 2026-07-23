#include "Logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QtLogging>

#include <cstdio>

namespace {
constexpr qint64 kMaxLogBytes = 2 * 1024 * 1024;

QMutex g_logMutex;
QFile g_logFile;
QtMessageHandler g_previousHandler = nullptr;

QString resolveLogFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("easy-ssh.log"));
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const QString line = qFormatLogMessage(type, context, msg);

    {
        QMutexLocker locker(&g_logMutex);
        if (g_logFile.isOpen()) {
            g_logFile.write(line.toUtf8());
            g_logFile.write("\n", 1);
            g_logFile.flush();
        }
    }

    if (g_previousHandler) {
        g_previousHandler(type, context, msg);
    } else {
        fprintf(stderr, "%s\n", qPrintable(line));
        fflush(stderr);
    }
}
} // namespace

Q_LOGGING_CATEGORY(lcSsh, "easy.ssh", QtWarningMsg)
Q_LOGGING_CATEGORY(lcApp, "easy.app", QtWarningMsg)

QString logFilePath()
{
    return resolveLogFilePath();
}

void initLogging()
{
    const QString path = resolveLogFilePath();
    const QString dir = QFileInfo(path).absolutePath();
    QDir().mkpath(dir);

    qSetMessagePattern(
        QStringLiteral("%{time yyyy-MM-dd hh:mm:ss.zzz} [%{type}] %{if-category}%{category}: "
                       "%{endif}%{message}"));

    QMutexLocker locker(&g_logMutex);
    if (g_logFile.isOpen()) {
        g_logFile.close();
    }

    g_logFile.setFileName(path);
    if (g_logFile.exists() && g_logFile.size() > kMaxLogBytes) {
        g_logFile.remove();
    }

    if (!g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    g_previousHandler = qInstallMessageHandler(messageHandler);
}
