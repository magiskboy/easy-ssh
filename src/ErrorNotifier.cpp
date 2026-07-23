#include "ErrorNotifier.h"

#include "Logging.h"

#include <QMessageBox>
#include <QObject>

namespace {
std::function<void(const QString &)> g_statusSink;
}

void ErrorNotifier::setStatusSink(std::function<void(const QString &)> sink)
{
    g_statusSink = std::move(sink);
}

void ErrorNotifier::notify(QWidget *parent, const QString &title, const QString &message,
                           Level level)
{
    const QString statusText = title.isEmpty() ? message : QStringLiteral("%1: %2").arg(title, message);

    if (g_statusSink) {
        g_statusSink(statusText);
    }

    switch (level) {
    case Level::Status:
        qCWarning(lcApp) << statusText;
        break;
    case Level::Warning:
        qCWarning(lcApp) << statusText;
        QMessageBox::warning(parent, title.isEmpty() ? QObject::tr("Warning") : title, message);
        break;
    case Level::Error:
        qCCritical(lcApp) << statusText;
        QMessageBox::critical(parent, title.isEmpty() ? QObject::tr("Error") : title, message);
        break;
    }
}
