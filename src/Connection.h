#pragma once

#include <QString>
#include <QUuid>

enum class AuthType {
    Password = 0,
    PrivateKey = 1,
};

struct Connection {
    QUuid id;
    QString name;
    QString host;
    quint16 port = 22;
    QString username;
    AuthType authType = AuthType::Password;
    QString privateKeyPath;
    QString startupDirectory;

    QString displayText() const
    {
        return QStringLiteral("%1 — %2@%3:%4")
            .arg(name, username, host)
            .arg(port);
    }
};
