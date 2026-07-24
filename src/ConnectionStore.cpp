#include "ConnectionStore.h"

#include <QSettings>

namespace
{

QString authTypeToString(AuthType type)
{
    switch (type) {
    case AuthType::PrivateKey:
        return QStringLiteral("privateKey");
    case AuthType::Password:
    default:
        return QStringLiteral("password");
    }
}

AuthType authTypeFromString(const QString &value)
{
    if (value == QLatin1String("privateKey")) {
        return AuthType::PrivateKey;
    }
    return AuthType::Password;
}

} // namespace

QList<Connection> ConnectionStore::load()
{
    QSettings settings;
    QList<Connection> connections;

    const int size = settings.beginReadArray(QStringLiteral("connections"));
    connections.reserve(size);

    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);

        Connection connection;
        connection.id = QUuid::fromString(settings.value(QStringLiteral("id")).toString());
        connection.name = settings.value(QStringLiteral("name")).toString();
        connection.host = settings.value(QStringLiteral("host")).toString();
        connection.port = static_cast<quint16>(settings.value(QStringLiteral("port"), 22).toUInt());
        connection.username = settings.value(QStringLiteral("username")).toString();
        connection.authType =
            authTypeFromString(settings.value(QStringLiteral("authType")).toString());
        connection.privateKeyPath = settings.value(QStringLiteral("privateKeyPath")).toString();
        connection.startupDirectory = settings.value(QStringLiteral("startupDirectory")).toString();

        if (connection.id.isNull() || connection.name.isEmpty()) {
            continue;
        }
        connections.append(connection);
    }

    settings.endArray();
    return connections;
}

void ConnectionStore::save(const QList<Connection> &connections)
{
    QSettings settings;
    settings.remove(QStringLiteral("connections"));
    settings.beginWriteArray(QStringLiteral("connections"), connections.size());

    for (int i = 0; i < connections.size(); ++i) {
        const Connection &connection = connections.at(i);
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("id"), connection.id.toString(QUuid::WithoutBraces));
        settings.setValue(QStringLiteral("name"), connection.name);
        settings.setValue(QStringLiteral("host"), connection.host);
        settings.setValue(QStringLiteral("port"), connection.port);
        settings.setValue(QStringLiteral("username"), connection.username);
        settings.setValue(QStringLiteral("authType"), authTypeToString(connection.authType));
        settings.setValue(QStringLiteral("privateKeyPath"), connection.privateKeyPath);
        settings.setValue(QStringLiteral("startupDirectory"), connection.startupDirectory);
    }

    settings.endArray();
    settings.sync();
}
