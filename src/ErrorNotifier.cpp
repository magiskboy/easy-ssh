#include "ErrorNotifier.h"

#include "Logging.h"

#include <QMessageBox>
#include <QObject>

namespace
{
std::function<void(const QString &, ErrorNotifier::Level)> g_statusSink;
}

void ErrorNotifier::setStatusSink(std::function<void(const QString &, Level)> sink)
{
    g_statusSink = std::move(sink);
}

void ErrorNotifier::status(const QString &message, Level level)
{
    if (g_statusSink) {
        g_statusSink(message, level);
    }

    switch (level) {
    case Level::Status:
    case Level::Success:
        qCInfo(lcApp) << message;
        break;
    case Level::Warning:
        qCWarning(lcApp) << message;
        break;
    case Level::Error:
        qCCritical(lcApp) << message;
        break;
    }
}

void ErrorNotifier::notify(QWidget *parent,
                           const QString &title,
                           const QString &message,
                           Level level)
{
    const QString statusText =
        title.isEmpty() ? message : QStringLiteral("%1: %2").arg(title, message);

    status(statusText, level);

    switch (level) {
    case Level::Status:
    case Level::Success:
        break;
    case Level::Warning:
        QMessageBox::warning(parent, title.isEmpty() ? QObject::tr("Warning") : title, message);
        break;
    case Level::Error:
        QMessageBox::critical(parent, title.isEmpty() ? QObject::tr("Error") : title, message);
        break;
    }
}
