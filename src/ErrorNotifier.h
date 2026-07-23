#pragma once

#include <QString>
#include <functional>

class QWidget;

class ErrorNotifier {
public:
    enum class Level {
        Status,
        Warning,
        Error,
    };

    static void setStatusSink(std::function<void(const QString &)> sink);
    static void notify(QWidget *parent, const QString &title, const QString &message,
                       Level level = Level::Warning);
};
