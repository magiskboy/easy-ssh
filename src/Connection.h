#pragma once

#include <QString>
#include <QUuid>

enum class AuthType
{
    Password = 0,
    PrivateKey = 1,
};

enum class ConnectionSource
{
    App = 0,
    SshConfig = 1,
};

struct Connection
{
    QUuid id;
    QString name;
    QString host;
    quint16 port = 22;
    QString username;
    AuthType authType = AuthType::Password;
    QString privateKeyPath;
    QString startupDirectory;
    ConnectionSource source = ConnectionSource::App;
    QString configAlias;

    QString displayText() const
    {
        QString text = QStringLiteral("%1 — %2@%3:%4").arg(name, username, host).arg(port);
        if (source == ConnectionSource::SshConfig) {
            text += QStringLiteral(" [ssh config]");
        }
        return text;
    }
};
