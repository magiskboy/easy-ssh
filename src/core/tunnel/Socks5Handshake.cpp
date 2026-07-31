// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "Socks5Handshake.h"

#include <QHostAddress>
#include <QTcpSocket>

namespace
{
constexpr quint8 kSocksVersion = 0x05;
constexpr quint8 kMethodNoAuth = 0x00;
constexpr quint8 kMethodUserPass = 0x02;
constexpr quint8 kMethodNoAcceptable = 0xff;
constexpr quint8 kCmdConnect = 0x01;
constexpr quint8 kAtypIpv4 = 0x01;
constexpr quint8 kAtypDomain = 0x03;
constexpr quint8 kAtypIpv6 = 0x04;
constexpr quint8 kRepSucceeded = 0x00;
constexpr quint8 kRepGeneralFailure = 0x01;
constexpr quint8 kRepCommandNotSupported = 0x07;
constexpr quint8 kRepAddressNotSupported = 0x08;
} // namespace

Socks5Handshake::Socks5Handshake(SocksAuthMode authMode, const Credentials &credentials)
    : m_authMode(authMode), m_username(credentials.username), m_password(credentials.password)
{
}

void Socks5Handshake::process(QTcpSocket *socket)
{
    if (socket == nullptr || isDone()) {
        return;
    }

    m_buffer.append(socket->readAll());

    while (!isDone()) {
        bool progressed = false;
        switch (m_state) {
        case State::WaitMethods:
            progressed = consumeMethods(socket);
            break;
        case State::WaitAuth:
            progressed = consumeAuth(socket);
            break;
        case State::WaitRequest:
            progressed = consumeRequest(socket);
            break;
        case State::Succeeded:
        case State::Failed:
            return;
        }
        if (!progressed) {
            return;
        }
    }
}

bool Socks5Handshake::consumeMethods(QTcpSocket *socket)
{
    if (m_buffer.size() < 2) {
        return false;
    }
    if (static_cast<quint8>(m_buffer.at(0)) != kSocksVersion) {
        fail(socket, QStringLiteral("Unsupported SOCKS version"));
        return true;
    }
    const int nmethods = static_cast<quint8>(m_buffer.at(1));
    if (m_buffer.size() < 2 + nmethods) {
        return false;
    }

    bool clientOffersNoAuth = false;
    bool clientOffersUserPass = false;
    for (int i = 0; i < nmethods; ++i) {
        const quint8 method = static_cast<quint8>(m_buffer.at(2 + i));
        if (method == kMethodNoAuth) {
            clientOffersNoAuth = true;
        } else if (method == kMethodUserPass) {
            clientOffersUserPass = true;
        }
    }
    m_buffer.remove(0, 2 + nmethods);

    QByteArray reply;
    reply.append(static_cast<char>(kSocksVersion));

    if (m_authMode == SocksAuthMode::UsernamePassword) {
        if (!clientOffersUserPass) {
            reply.append(static_cast<char>(kMethodNoAcceptable));
            socket->write(reply);
            fail(socket, QStringLiteral("Client did not offer username/password auth"), {});
            return true;
        }
        reply.append(static_cast<char>(kMethodUserPass));
        socket->write(reply);
        m_state = State::WaitAuth;
        return true;
    }

    if (!clientOffersNoAuth) {
        reply.append(static_cast<char>(kMethodNoAcceptable));
        socket->write(reply);
        fail(socket, QStringLiteral("Client did not offer no-auth method"), {});
        return true;
    }
    reply.append(static_cast<char>(kMethodNoAuth));
    socket->write(reply);
    m_state = State::WaitRequest;
    return true;
}

bool Socks5Handshake::consumeAuth(QTcpSocket *socket)
{
    // RFC 1929: VER(1) ULEN(1) UNAME ULEN PASS
    if (m_buffer.size() < 2) {
        return false;
    }
    const quint8 ver = static_cast<quint8>(m_buffer.at(0));
    const int ulen = static_cast<quint8>(m_buffer.at(1));
    if (m_buffer.size() < 2 + ulen + 1) {
        return false;
    }
    const int plen = static_cast<quint8>(m_buffer.at(2 + ulen));
    if (m_buffer.size() < 2 + ulen + 1 + plen) {
        return false;
    }

    const QString user = QString::fromUtf8(m_buffer.mid(2, ulen));
    const QString pass = QString::fromUtf8(m_buffer.mid(2 + ulen + 1, plen));
    m_buffer.remove(0, 2 + ulen + 1 + plen);

    QByteArray reply;
    reply.append(char(0x01)); // auth version
    const bool ok = (ver == 0x01) && (user == m_username) && (pass == m_password);
    reply.append(ok ? char(0x00) : char(0x01));
    socket->write(reply);

    if (!ok) {
        fail(socket, QStringLiteral("SOCKS authentication failed"), {});
        return true;
    }
    m_state = State::WaitRequest;
    return true;
}

bool Socks5Handshake::consumeRequest(QTcpSocket *socket)
{
    // VER CMD RSV ATYP ...
    if (m_buffer.size() < 4) {
        return false;
    }
    const quint8 ver = static_cast<quint8>(m_buffer.at(0));
    const quint8 cmd = static_cast<quint8>(m_buffer.at(1));
    const quint8 atyp = static_cast<quint8>(m_buffer.at(3));
    if (ver != kSocksVersion) {
        fail(socket, QStringLiteral("Invalid SOCKS request version"));
        return true;
    }
    if (cmd != kCmdConnect) {
        QByteArray reply;
        reply.append(char(kSocksVersion));
        reply.append(char(kRepCommandNotSupported));
        reply.append(char(0x00));
        reply.append(char(kAtypIpv4));
        reply.append(QByteArray(4 + 2, '\0'));
        socket->write(reply);
        fail(socket, QStringLiteral("Only SOCKS CONNECT is supported"), {});
        return true;
    }

    int offset = 4;
    QString host;
    if (atyp == kAtypIpv4) {
        if (m_buffer.size() < offset + 4 + 2) {
            return false;
        }
        const quint32 addr =
            (quint8(m_buffer.at(offset)) << 24) | (quint8(m_buffer.at(offset + 1)) << 16) |
            (quint8(m_buffer.at(offset + 2)) << 8) | quint8(m_buffer.at(offset + 3));
        host = QHostAddress(addr).toString();
        offset += 4;
    } else if (atyp == kAtypDomain) {
        if (m_buffer.size() < offset + 1) {
            return false;
        }
        const int len = static_cast<quint8>(m_buffer.at(offset));
        ++offset;
        if (m_buffer.size() < offset + len + 2) {
            return false;
        }
        host = QString::fromUtf8(m_buffer.mid(offset, len));
        offset += len;
    } else if (atyp == kAtypIpv6) {
        if (m_buffer.size() < offset + 16 + 2) {
            return false;
        }
        const QByteArray raw = m_buffer.mid(offset, 16);
        host = QHostAddress(reinterpret_cast<const quint8 *>(raw.constData())).toString();
        offset += 16;
    } else {
        QByteArray reply;
        reply.append(char(kSocksVersion));
        reply.append(char(kRepAddressNotSupported));
        reply.append(char(0x00));
        reply.append(char(kAtypIpv4));
        reply.append(QByteArray(4 + 2, '\0'));
        socket->write(reply);
        fail(socket, QStringLiteral("Unsupported address type"), {});
        return true;
    }

    const quint16 port = (quint8(m_buffer.at(offset)) << 8) | quint8(m_buffer.at(offset + 1));
    offset += 2;
    m_buffer.remove(0, offset);

    m_destHost = host;
    m_destPort = port;
    m_state = State::Succeeded;
    return true;
}

void Socks5Handshake::fail(QTcpSocket *socket, const QString &error, const QByteArray &reply)
{
    Q_UNUSED(socket);
    if (!reply.isEmpty() && socket) {
        socket->write(reply);
    }
    m_error = error;
    m_state = State::Failed;
}

void Socks5Handshake::writeConnectReply(QTcpSocket *socket, bool success)
{
    if (socket == nullptr) {
        return;
    }
    QByteArray reply;
    reply.append(char(kSocksVersion));
    reply.append(char(success ? kRepSucceeded : kRepGeneralFailure));
    reply.append(char(0x00)); // RSV
    reply.append(char(kAtypIpv4));
    reply.append(QByteArray(4, '\0')); // bind addr 0.0.0.0
    reply.append(char(0x00));
    reply.append(char(0x00)); // bind port 0
    socket->write(reply);
}
