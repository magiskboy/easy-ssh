/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#import "ESSSessionController.h"
#import "ESSConnectionMapping.h"
#import "EasySshRuntime.h"

#include "core/connection/Connection.h"
#include "core/ssh/SshWorker.h"
#include "core/tunnel/Tunnel.h"

#include <QMetaObject>
#include <QThread>
#include <QUuid>

#include <utility>

namespace {

NSString *qToNS(const QString &s)
{
    return [[NSString alloc] initWithUTF8String:s.toUtf8().constData()];
}

QString nsToQ(NSString *s)
{
    if (s == nil) {
        return {};
    }
    return QString::fromUtf8([s UTF8String]);
}

NSUUID *uuidToNS(const QUuid &id)
{
    if (id.isNull()) {
        return nil;
    }
    return [[NSUUID alloc] initWithUUIDString:qToNS(id.toString(QUuid::WithoutBraces))];
}

QUuid nsToUuid(NSUUID *id)
{
    if (id == nil) {
        return {};
    }
    return QUuid(nsToQ(id.UUIDString));
}

NSArray<NSString *> *nsStringArray(NSArray<NSString *> *arr)
{
    return arr ?: @[];
}

QStringList toQStringList(NSArray<NSString *> *arr)
{
    QStringList out;
    for (NSString *s in nsStringArray(arr)) {
        out.append(nsToQ(s));
    }
    return out;
}

void dispatchMain(dispatch_block_t block)
{
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_async(dispatch_get_main_queue(), block);
    }
}

} // namespace

@interface ESSSessionController ()
@property (nonatomic, strong, nullable) NSUUID *primaryShellId;
@property (nonatomic, assign) BOOL connected;
@end

@implementation ESSSessionController {
    QThread *m_thread;
    SshWorker *m_worker;
    Connection m_connection;
    SessionCredentials m_credentials;
    bool m_shuttingDown;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        [EasySshRuntime start];
        m_thread = nullptr;
        m_worker = nullptr;
        m_shuttingDown = false;
        _connected = NO;
    }
    return self;
}

- (void)dealloc
{
    [self shutdownWorker];
}

- (void)shutdownWorker
{
    m_shuttingDown = true;
    if (m_worker != nullptr) {
        QObject::disconnect(m_worker, nullptr, nullptr, nullptr);
        m_worker->requestCancel();
        if (m_thread != nullptr && m_thread->isRunning()) {
            QMetaObject::invokeMethod(
                m_worker,
                [worker = m_worker]() { worker->disconnectSession(); },
                Qt::BlockingQueuedConnection);
        } else {
            m_worker->disconnectSession();
        }
    }
    if (m_thread != nullptr) {
        m_thread->quit();
        if (!m_thread->wait(2000)) {
            m_thread->terminate();
            m_thread->wait(1000);
        }
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_worker;
    m_worker = nullptr;
    self.connected = NO;
    self.primaryShellId = nil;
}

- (void)wireWorkerSignals
{
    QObject::connect(m_worker, &SshWorker::connected, m_worker, [self](const QUuid &shellId) {
        NSUUID *nsId = uuidToNS(shellId);
        dispatchMain(^{
            self.primaryShellId = nsId;
            self.connected = YES;
            if (self.onConnected) {
                self.onConnected(nsId);
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::dataReceived, m_worker,
                     [self](const QUuid &shellId, const QByteArray &data) {
                         if (data.isEmpty()) {
                             return;
                         }
                         NSUUID *nsId = uuidToNS(shellId);
                         NSData *nsData = [NSData dataWithBytes:data.constData() length:static_cast<NSUInteger>(data.size())];
                         dispatchMain(^{
                             if (self.onData) {
                                 self.onData(nsId, nsData);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::shellOpened, m_worker, [self](const QUuid &shellId) {
        NSUUID *nsId = uuidToNS(shellId);
        dispatchMain(^{
            if (self.onShellOpened) {
                self.onShellOpened(nsId);
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::shellClosed, m_worker, [self](const QUuid &shellId) {
        NSUUID *nsId = uuidToNS(shellId);
        dispatchMain(^{
            if (self.primaryShellId != nil && [self.primaryShellId isEqual:nsId]) {
                self.primaryShellId = nil;
            }
            if (self.onShellClosed) {
                self.onShellClosed(nsId);
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::shellFailed, m_worker,
                     [self](const QUuid &shellId, const QString &message) {
                         NSUUID *nsId = uuidToNS(shellId);
                         NSString *msg = qToNS(message);
                         dispatchMain(^{
                             if (self.onShellFailed) {
                                 self.onShellFailed(nsId, msg);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::hostKeyPrompt, m_worker,
                     [self](SshWorker::HostKeyPrompt reason, const QString &fingerprint,
                            const QString &contextLabel) {
                         ESSHostKeyPromptReason r = ESSHostKeyPromptReasonUnknown;
                         switch (reason) {
                         case SshWorker::HostKeyPrompt::Changed:
                             r = ESSHostKeyPromptReasonChanged;
                             break;
                         case SshWorker::HostKeyPrompt::Other:
                             r = ESSHostKeyPromptReasonOther;
                             break;
                         case SshWorker::HostKeyPrompt::Unknown:
                         default:
                             r = ESSHostKeyPromptReasonUnknown;
                             break;
                         }
                         NSString *fp = qToNS(fingerprint);
                         NSString *ctx = qToNS(contextLabel);
                         dispatchMain(^{
                             if (self.onHostKeyPrompt) {
                                 self.onHostKeyPrompt(r, fp, ctx);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::errorOccurred, m_worker, [self](const QString &message) {
        NSString *msg = qToNS(message);
        dispatchMain(^{
            self.connected = NO;
            if (self.onError) {
                self.onError(msg);
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::disconnected, m_worker, [self]() {
        dispatchMain(^{
            self.connected = NO;
            self.primaryShellId = nil;
            if (self.onDisconnected) {
                self.onDisconnected();
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::agentForwardingWarning, m_worker,
                     [self](const QString &message) {
                         NSString *msg = qToNS(message);
                         dispatchMain(^{
                             if (self.onAgentForwardingWarning) {
                                 self.onAgentForwardingWarning(msg);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::directoryListed, m_worker,
                     [self](const QString &path, const QVector<RemoteEntry> &entries) {
                         NSString *nsPath = qToNS(path);
                         NSMutableArray *arr = [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(entries.size())];
                         for (const RemoteEntry &e : entries) {
                             [arr addObject:@{
                                 @"name" : qToNS(e.name),
                                 @"path" : qToNS(e.path),
                                 @"isDir" : @(e.isDir),
                                 @"isSymlink" : @(e.isSymlink),
                                 @"linkIsDir" : @(e.linkIsDir),
                                 @"linkTarget" : qToNS(e.linkTarget),
                                 @"size" : @(e.size),
                                 @"permissions" : qToNS(e.permissions),
                                 @"mtime" : @(e.mtime),
                             }];
                         }
                         dispatchMain(^{
                             if (self.onDirectoryListed) {
                                 self.onDirectoryListed(nsPath, arr);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::entryResolved, m_worker,
                     [self](const QString &path, bool isDir, bool ok, const QString &error) {
                         NSString *nsPath = qToNS(path);
                         NSString *nsErr = qToNS(error);
                         dispatchMain(^{
                             if (self.onEntryResolved) {
                                 self.onEntryResolved(nsPath, isDir, ok, nsErr);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::pathCanonicalized, m_worker,
                     [self](const QString &requested, const QString &canonical) {
                         NSString *a = qToNS(requested);
                         NSString *b = qToNS(canonical);
                         dispatchMain(^{
                             if (self.onPathCanonicalized) {
                                 self.onPathCanonicalized(a, b);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::sftpFinished, m_worker, [self](const QString &message) {
        NSString *msg = qToNS(message);
        dispatchMain(^{
            if (self.onSftpFinished) {
                self.onSftpFinished(msg);
            }
        });
    });
    QObject::connect(m_worker, &SshWorker::sftpError, m_worker, [self](const QString &message) {
        NSString *msg = qToNS(message);
        dispatchMain(^{
            if (self.onSftpError) {
                self.onSftpError(msg);
            }
        });
    });
    QObject::connect(m_worker, &SshWorker::sftpCanceled, m_worker, [self](const QString &message) {
        NSString *msg = qToNS(message);
        dispatchMain(^{
            if (self.onSftpCanceled) {
                self.onSftpCanceled(msg);
            }
        });
    });
    QObject::connect(m_worker, &SshWorker::sftpUnavailable, m_worker, [self](const QString &message) {
        NSString *msg = qToNS(message);
        dispatchMain(^{
            if (self.onSftpUnavailable) {
                self.onSftpUnavailable(msg);
            }
        });
    });
    QObject::connect(m_worker, &SshWorker::sftpProgress, m_worker,
                     [self](qint64 done, qint64 total, const QString &name) {
                         NSString *nsName = qToNS(name);
                         dispatchMain(^{
                             if (self.onSftpProgress) {
                                 self.onSftpProgress(done, total, nsName);
                             }
                         });
                     });
    QObject::connect(m_worker, &SshWorker::remoteFsOpened, m_worker, [self](int backend) {
        dispatchMain(^{
            if (self.onRemoteFsOpened) {
                self.onRemoteFsOpened(backend);
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::tunnelStatusChanged, m_worker,
                     [self](const QUuid &tunnelId, const QString &status, const QString &detail) {
                         NSUUID *nsId = uuidToNS(tunnelId);
                         NSString *st = qToNS(status);
                         NSString *dt = qToNS(detail);
                         dispatchMain(^{
                             if (self.onTunnelStatusChanged) {
                                 self.onTunnelStatusChanged(nsId, st, dt);
                             }
                         });
                     });
    QObject::connect(m_worker, &SshWorker::tunnelError, m_worker,
                     [self](const QUuid &tunnelId, const QString &message) {
                         NSUUID *nsId = uuidToNS(tunnelId);
                         NSString *msg = qToNS(message);
                         dispatchMain(^{
                             if (self.onTunnelError) {
                                 self.onTunnelError(nsId, msg);
                             }
                         });
                     });

    QObject::connect(
        m_worker, &SshWorker::commandFinished, m_worker,
        [self](const QString &requestId, int exitStatus, const QByteArray &stdoutBytes,
               const QByteArray &stderrBytes, const QString &errorMessage) {
            NSString *rid = qToNS(requestId);
            NSData *outData = [NSData dataWithBytes:stdoutBytes.constData()
                                             length:static_cast<NSUInteger>(stdoutBytes.size())];
            NSData *errData = [NSData dataWithBytes:stderrBytes.constData()
                                             length:static_cast<NSUInteger>(stderrBytes.size())];
            NSString *err = qToNS(errorMessage);
            dispatchMain(^{
                if (self.onCommandFinished) {
                    self.onCommandFinished(rid, exitStatus, outData, errData, err);
                }
            });
        });
}

- (void)connectWithConnection:(ESSConnectionInfo *)connection
                  credentials:(ESSSessionCredentials *)credentials
                         cols:(NSInteger)cols
                         rows:(NSInteger)rows
{
    [EasySshRuntime start];
    [self shutdownWorker];
    m_shuttingDown = false;

    m_connection = essConnectionFromInfo(connection);
    if (m_connection.name.isEmpty()) {
        m_connection.name =
            QStringLiteral("%1@%2").arg(m_connection.username, m_connection.host);
    }
    m_credentials = essCredentialsFromInfo(credentials);

    m_thread = new QThread();
    m_worker = new SshWorker();
    m_worker->moveToThread(m_thread);

    [self wireWorkerSignals];

    m_thread->start();

    const int useCols = cols > 0 ? static_cast<int>(cols) : 80;
    const int useRows = rows > 0 ? static_cast<int>(rows) : 24;
    const QUuid shellId = QUuid::createUuid();
    self.primaryShellId = uuidToNS(shellId);

    const Connection conn = m_connection;
    const SessionCredentials creds = m_credentials;
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, conn, creds, shellId, useCols, useRows]() {
            worker->connectToHost(conn, creds, shellId, useCols, useRows);
        },
        Qt::QueuedConnection);
}

- (void)connectWithHost:(NSString *)host
                   port:(NSInteger)port
               username:(NSString *)username
               authType:(ESSAuthType)authType
               password:(NSString *)password
         privateKeyPath:(NSString *)privateKeyPath
                   cols:(NSInteger)cols
                   rows:(NSInteger)rows
{
    ESSConnectionInfo *info = [[ESSConnectionInfo alloc] init];
    info.connectionId = [NSUUID UUID];
    info.host = host ?: @"";
    info.port = port > 0 ? port : 22;
    info.username = username ?: @"";
    info.name = [NSString stringWithFormat:@"%@@%@", info.username, info.host];
    info.authType = authType;
    info.privateKeyPath = privateKeyPath;

    ESSSessionCredentials *creds = [[ESSSessionCredentials alloc] init];
    creds.targetSecret = password;

    [self connectWithConnection:info credentials:creds cols:cols rows:rows];
}

- (void)disconnect
{
    [self shutdownWorker];
    dispatchMain(^{
        if (self.onDisconnected) {
            self.onDisconnected();
        }
    });
}

- (void)reconnectWithCols:(NSInteger)cols rows:(NSInteger)rows
{
    if (m_connection.host.isEmpty()) {
        return;
    }
    ESSConnectionInfo *info = essConnectionToInfo(m_connection);
    ESSSessionCredentials *creds = [[ESSSessionCredentials alloc] init];
    if (!m_credentials.targetSecret.isEmpty()) {
        creds.targetSecret = qToNS(m_credentials.targetSecret);
    }
    if (!m_credentials.gatewaySecret.isEmpty()) {
        creds.gatewaySecret = qToNS(m_credentials.gatewaySecret);
    }
    [self connectWithConnection:info credentials:creds cols:cols rows:rows];
}

- (void)respondHostKeyTrust:(BOOL)accept
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, accept]() { worker->respondHostKeyTrust(accept); },
        Qt::QueuedConnection);
}

- (QUuid)resolvedShellId:(NSUUID *)shellId
{
    if (shellId != nil) {
        return nsToUuid(shellId);
    }
    return nsToUuid(self.primaryShellId);
}

- (void)writeData:(NSData *)data shellId:(NSUUID *)shellId
{
    if (m_worker == nullptr || data == nil || data.length == 0) {
        return;
    }
    const QUuid id = [self resolvedShellId:shellId];
    const QByteArray bytes(static_cast<const char *>(data.bytes), static_cast<int>(data.length));
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, id, bytes]() { worker->writeToChannel(id, bytes); },
        Qt::QueuedConnection);
}

- (void)resizeCols:(NSInteger)cols rows:(NSInteger)rows shellId:(NSUUID *)shellId
{
    if (m_worker == nullptr) {
        return;
    }
    const QUuid id = [self resolvedShellId:shellId];
    const int c = static_cast<int>(cols);
    const int r = static_cast<int>(rows);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, id, c, r]() { worker->changePtySize(id, c, r); },
        Qt::QueuedConnection);
}

- (void)openShell:(NSUUID *)shellId cols:(NSInteger)cols rows:(NSInteger)rows
{
    if (m_worker == nullptr || shellId == nil) {
        return;
    }
    const QUuid id = nsToUuid(shellId);
    const int c = static_cast<int>(cols);
    const int r = static_cast<int>(rows);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, id, c, r]() { worker->openShell(id, c, r); },
        Qt::QueuedConnection);
}

- (void)closeShell:(NSUUID *)shellId
{
    if (m_worker == nullptr || shellId == nil) {
        return;
    }
    const QUuid id = nsToUuid(shellId);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, id]() { worker->closeShell(id); }, Qt::QueuedConnection);
}

- (void)listDirectory:(NSString *)path
{
    if (m_worker == nullptr) {
        return;
    }
    const QString p = nsToQ(path);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, p]() { worker->listDirectory(p); }, Qt::QueuedConnection);
}

- (void)createDirectory:(NSString *)path
{
    if (m_worker == nullptr) {
        return;
    }
    const QString p = nsToQ(path);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, p]() { worker->createDirectory(p); }, Qt::QueuedConnection);
}

- (void)createSymlink:(NSString *)target linkPath:(NSString *)linkPath
{
    if (m_worker == nullptr) {
        return;
    }
    const QString t = nsToQ(target);
    const QString l = nsToQ(linkPath);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, t, l]() { worker->createSymlink(t, l); }, Qt::QueuedConnection);
}

- (void)resolveEntry:(NSString *)path
{
    if (m_worker == nullptr) {
        return;
    }
    const QString p = nsToQ(path);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, p]() { worker->resolveEntry(p); }, Qt::QueuedConnection);
}

- (void)renamePath:(NSString *)from to:(NSString *)to
{
    if (m_worker == nullptr) {
        return;
    }
    const QString a = nsToQ(from);
    const QString b = nsToQ(to);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, a, b]() { worker->renamePath(a, b); }, Qt::QueuedConnection);
}

- (void)removePath:(NSString *)path recursive:(BOOL)recursive
{
    if (m_worker == nullptr) {
        return;
    }
    const QString p = nsToQ(path);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, p, recursive]() { worker->removePath(p, recursive); },
        Qt::QueuedConnection);
}

- (void)uploadFiles:(NSArray<NSString *> *)localPaths remoteDir:(NSString *)remoteDir
{
    if (m_worker == nullptr) {
        return;
    }
    const QStringList paths = toQStringList(localPaths);
    const QString dir = nsToQ(remoteDir);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, paths, dir]() { worker->uploadFiles(paths, dir); },
        Qt::QueuedConnection);
}

- (void)uploadFileTo:(NSString *)localPath remotePath:(NSString *)remotePath
{
    if (m_worker == nullptr) {
        return;
    }
    const QString local = nsToQ(localPath);
    const QString remote = nsToQ(remotePath);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, local, remote]() { worker->uploadFileTo(local, remote); },
        Qt::QueuedConnection);
}

- (void)downloadPaths:(NSArray<NSString *> *)remotePaths localDir:(NSString *)localDir
{
    if (m_worker == nullptr) {
        return;
    }
    const QStringList paths = toQStringList(remotePaths);
    const QString dir = nsToQ(localDir);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, paths, dir]() { worker->downloadPaths(paths, dir); },
        Qt::QueuedConnection);
}

- (void)canonicalizePath:(NSString *)path
{
    if (m_worker == nullptr) {
        return;
    }
    const QString p = nsToQ(path);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, p]() { worker->canonicalizePath(p); }, Qt::QueuedConnection);
}

- (void)cancelTransfer
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker]() { worker->cancelTransfer(); }, Qt::QueuedConnection);
}

- (void)startLocalTunnelNamed:(NSString *)name
                    localHost:(NSString *)localHost
                    localPort:(NSInteger)localPort
                   remoteHost:(NSString *)remoteHost
                   remotePort:(NSInteger)remotePort
{
    if (m_worker == nullptr) {
        return;
    }
    TunnelDefinition def;
    def.id = QUuid::createUuid();
    def.connectionId = m_connection.id;
    def.name = nsToQ(name);
    def.type = TunnelType::Local;
    def.enabled = true;
    def.localHost = nsToQ(localHost);
    def.localPort = static_cast<quint16>(localPort);
    def.remoteHost = nsToQ(remoteHost);
    def.remotePort = static_cast<quint16>(remotePort);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, def]() { worker->startTunnel(def); }, Qt::QueuedConnection);
}

- (void)stopTunnel:(NSUUID *)tunnelId
{
    if (m_worker == nullptr || tunnelId == nil) {
        return;
    }
    const QUuid id = nsToUuid(tunnelId);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, id]() { worker->stopTunnel(id); }, Qt::QueuedConnection);
}

- (void)stopAllTunnels
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker]() { worker->stopAllTunnels(); }, Qt::QueuedConnection);
}

- (void)execCommand:(NSString *)command requestId:(NSString *)requestId
{
    if (m_worker == nullptr) {
        return;
    }
    const QString cmd = nsToQ(command);
    const QString rid = nsToQ(requestId);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, rid, cmd]() { worker->execCommand(rid, cmd); },
        Qt::QueuedConnection);
}

@end
