/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/tunnel/Tunnel.h"

#include <QByteArray>
#include <QString>

class QTcpSocket;

/**
 * SOCKS5 handshake state machine (RFC 1928 + RFC 1929 username/password).
 * Owns buffering until CONNECT succeeds or fails; caller then uses the socket as a byte pipe.
 */
class Socks5Handshake
{
public:
    enum class State
    {
        WaitMethods,
        WaitAuth,
        WaitRequest,
        Succeeded,
        Failed,
    };

    struct Credentials
    {
        QString username;
        QString password;
    };

    explicit Socks5Handshake(SocksAuthMode authMode, const Credentials &credentials);

    State state() const { return m_state; }
    bool isDone() const { return m_state == State::Succeeded || m_state == State::Failed; }
    bool succeeded() const { return m_state == State::Succeeded; }

    QString destHost() const { return m_destHost; }
    quint16 destPort() const { return m_destPort; }
    QString errorString() const { return m_error; }

    /// Feed newly readable socket data. May write replies to the socket.
    void process(QTcpSocket *socket);

    /// Write a SOCKS5 CONNECT reply (after SSH forward open success/failure).
    static void writeConnectReply(QTcpSocket *socket, bool success);

private:
    bool consumeMethods(QTcpSocket *socket);
    bool consumeAuth(QTcpSocket *socket);
    bool consumeRequest(QTcpSocket *socket);
    void fail(QTcpSocket *socket, const QString &error, const QByteArray &reply = {});

    SocksAuthMode m_authMode = SocksAuthMode::None;
    QString m_username;
    QString m_password;
    State m_state = State::WaitMethods;
    QByteArray m_buffer;
    QString m_destHost;
    quint16 m_destPort = 0;
    QString m_error;
};
