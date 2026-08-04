/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "core/session/WorkspaceState.h"

#include <QtTest>

class WorkspaceStateTest final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripPreservesFields();
    void fromJsonRejectsInvalidPayload();
    void sessionForLookup();
};

void WorkspaceStateTest::roundTripPreservesFields()
{
    WorkspaceState state;
    state.activeConnectionId = QUuid::createUuid();

    WorkspaceSessionEntry session;
    session.connectionId = state.activeConnectionId;
    session.activeShellId = QUuid::createUuid();
    session.dockState = QByteArrayLiteral("dock-bytes");
    session.shells.append(WorkspaceShellEntry{session.activeShellId, QStringLiteral("Shell 1")});
    session.shells.append(WorkspaceShellEntry{QUuid{}, QStringLiteral("ignored")});
    state.sessions.append(session);

    WorkspaceSessionEntry nullSession;
    nullSession.connectionId = QUuid{};
    state.sessions.append(nullSession);

    bool ok = false;
    const WorkspaceState loaded = WorkspaceState::fromJson(state.toJson(), &ok);
    QVERIFY(ok);
    QCOMPARE(loaded.version, WorkspaceState::kCurrentVersion);
    QCOMPARE(loaded.activeConnectionId, state.activeConnectionId);
    QCOMPARE(loaded.sessions.size(), 1);
    QCOMPARE(loaded.sessions.first().shells.size(), 1);
    QCOMPARE(loaded.sessions.first().shells.first().title, QStringLiteral("Shell 1"));
    QCOMPARE(loaded.sessions.first().dockState, QByteArrayLiteral("dock-bytes"));
}

void WorkspaceStateTest::fromJsonRejectsInvalidPayload()
{
    bool ok = true;
    WorkspaceState empty = WorkspaceState::fromJson(QByteArray(), &ok);
    QVERIFY(!ok);
    QVERIFY(empty.isEmpty());

    ok = true;
    empty = WorkspaceState::fromJson(QByteArrayLiteral("{oops}"), &ok);
    QVERIFY(!ok);

    ok = true;
    empty = WorkspaceState::fromJson(QByteArrayLiteral("{\"v\":99,\"sessions\":[]}"), &ok);
    QVERIFY(!ok);
}

void WorkspaceStateTest::sessionForLookup()
{
    WorkspaceState state;
    WorkspaceSessionEntry session;
    session.connectionId = QUuid::createUuid();
    state.sessions.append(session);

    QVERIFY(state.sessionFor(QUuid{}) == nullptr);
    QVERIFY(state.sessionFor(QUuid::createUuid()) == nullptr);
    QCOMPARE(state.sessionFor(session.connectionId)->connectionId, session.connectionId);
}

QTEST_GUILESS_MAIN(WorkspaceStateTest)

#include "tst_WorkspaceState.moc"
