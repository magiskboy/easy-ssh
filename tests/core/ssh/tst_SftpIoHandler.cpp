// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/fs/SftpAioTransfer.h"
#include "core/fs/SftpMetaIoHandler.h"
#include "core/fs/SftpTransferIoHandler.h"
#include "core/ssh/SshIoLoop.h"

#include <QtTest>

#include <memory>

class SftpIoHandlerTest final : public QObject
{
    Q_OBJECT

private slots:
    void metaHandlerIdIsUuid();
    void metaCancelBeforeStartIsIdempotent();
    void transferCancelBeforeStartIsIdempotent();
    void metaHandlerRemovedAfterCompleteHook();
    void aioTransferDefaultInactive();
};

// Manual regression (p9 Phase 3): large directory browse + terminal; upload/download
// + cancel; shell must not starve on SFTP path. SCP fallback remains sync (Phase 5).

void SftpIoHandlerTest::metaHandlerIdIsUuid()
{
    SftpMetaIoHandler::Request req;
    req.op = SftpMetaIoHandler::Op::ListDirectory;
    req.path = QStringLiteral("/");
    SftpMetaIoHandler handler(nullptr, req, {});
    QVERIFY(!handler.id().isEmpty());
    QCOMPARE(handler.id().size(), 36); // UUID WithoutBraces
}

void SftpIoHandlerTest::metaCancelBeforeStartIsIdempotent()
{
    SftpMetaIoHandler::Request req;
    req.op = SftpMetaIoHandler::Op::CreateDirectory;
    req.path = QStringLiteral("/tmp/x");
    int completed = 0;
    SftpMetaIoHandler::Hooks hooks;
    hooks.completed = [&]() { ++completed; };
    SftpMetaIoHandler handler(nullptr, req, hooks);
    handler.cancel();
    handler.cancel();
    // Not started → cancel must not invoke completed (avoids FS queue races).
    QCOMPARE(completed, 0);
}

void SftpIoHandlerTest::transferCancelBeforeStartIsIdempotent()
{
    SftpTransferIoHandler::Request req;
    req.kind = SftpTransferIoHandler::Kind::UploadFiles;
    int completed = 0;
    SftpTransferIoHandler::Hooks hooks;
    hooks.completed = [&]() { ++completed; };
    SftpTransferIoHandler handler(nullptr, req, hooks);
    handler.cancel();
    handler.cancel();
    QCOMPARE(completed, 0);
}

void SftpIoHandlerTest::metaHandlerRemovedAfterCompleteHook()
{
    SshIoLoop loop;
    int completed = 0;
    SftpMetaIoHandler::Request req;
    req.op = SftpMetaIoHandler::Op::CanonicalizePath;
    req.path = QStringLiteral(".");

    auto handler = std::make_unique<SftpMetaIoHandler>(nullptr, req, SftpMetaIoHandler::Hooks{});
    const QString id = handler->id();
    handler->setCompletedHook([&]() { ++completed; });
    QString error;
    QVERIFY(!loop.addHandler(std::move(handler), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(completed, 0);
    QCOMPARE(loop.handler(id), nullptr);
}

void SftpIoHandlerTest::aioTransferDefaultInactive()
{
    SftpAioTransfer xfer;
    QVERIFY(!xfer.isActive());
    QCOMPARE(xfer.partialBytes(), 0);
    xfer.abort();
    QVERIFY(!xfer.isActive());
}

QTEST_GUILESS_MAIN(SftpIoHandlerTest)
#include "tst_SftpIoHandler.moc"
