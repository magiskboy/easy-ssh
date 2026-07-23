#pragma once

#include "Connection.h"

#include <QList>

class ConnectionStore {
public:
    static QList<Connection> load();
    static void save(const QList<Connection> &connections);
};
