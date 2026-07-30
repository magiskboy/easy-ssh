#pragma once

#include <QLoggingCategory>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(lcSsh)
Q_DECLARE_LOGGING_CATEGORY(lcApp)

void initLogging();
QString logFilePath();
