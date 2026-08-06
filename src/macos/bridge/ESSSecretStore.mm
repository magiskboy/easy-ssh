/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#import "ESSSecretStore.h"
#import "EasySshRuntime.h"

#include "core/connection/SecretStore.h"

#include <QMetaObject>
#include <QUuid>

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

QUuid nsToUuid(NSUUID *id)
{
    if (id == nil) {
        return {};
    }
    return QUuid(nsToQ(id.UUIDString));
}

SecretStore::Kind toKind(ESSSecretKind kind)
{
    switch (kind) {
    case ESSSecretKindPassphrase:
        return SecretStore::Kind::Passphrase;
    case ESSSecretKindGatewayPassword:
        return SecretStore::Kind::GatewayPassword;
    case ESSSecretKindGatewayPassphrase:
        return SecretStore::Kind::GatewayPassphrase;
    case ESSSecretKindTunnelSocksPassword:
        return SecretStore::Kind::TunnelSocksPassword;
    case ESSSecretKindPassword:
    default:
        return SecretStore::Kind::Password;
    }
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

@implementation ESSSecretStore {
    SecretStore *m_store;
}

+ (ESSSecretStore *)sharedStore
{
    [EasySshRuntime start];
    static ESSSecretStore *shared = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shared = [[ESSSecretStore alloc] init];
    });
    return shared;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        [EasySshRuntime start];
        m_store = new SecretStore();
    }
    return self;
}

- (void)dealloc
{
    delete m_store;
    m_store = nullptr;
}

- (void)storeSecretForId:(NSUUID *)itemId
                    kind:(ESSSecretKind)kind
                   value:(NSString *)value
              completion:(ESSSecretBoolCompletion)completion
{
    const QUuid id = nsToUuid(itemId);
    const SecretStore::Kind k = toKind(kind);
    const QString text = nsToQ(value);

    QObject::connect(
        m_store, &SecretStore::storeFinished, m_store,
        [completion, id, k](const QUuid &connectionId, SecretStore::Kind finishedKind, bool ok,
                            const QString &error) {
            if (connectionId != id || finishedKind != k) {
                return;
            }
            NSString *err = qToNS(error);
            dispatchMain(^{
                if (completion) {
                    completion(ok, err);
                }
            });
        },
        Qt::SingleShotConnection);

    m_store->storeSecret(id, k, text);
}

- (void)readSecretForId:(NSUUID *)itemId
                   kind:(ESSSecretKind)kind
             completion:(ESSSecretReadCompletion)completion
{
    const QUuid id = nsToUuid(itemId);
    const SecretStore::Kind k = toKind(kind);

    QObject::connect(
        m_store, &SecretStore::readFinished, m_store,
        [completion, id, k](const QUuid &connectionId, SecretStore::Kind finishedKind,
                            const QString &value, bool ok, const QString &error) {
            if (connectionId != id || finishedKind != k) {
                return;
            }
            NSString *nsValue = value.isEmpty() ? nil : qToNS(value);
            NSString *err = qToNS(error);
            dispatchMain(^{
                if (completion) {
                    completion(nsValue, ok, err);
                }
            });
        },
        Qt::SingleShotConnection);

    m_store->readSecret(id, k);
}

- (void)deleteSecretForId:(NSUUID *)itemId
                     kind:(ESSSecretKind)kind
               completion:(ESSSecretBoolCompletion)completion
{
    const QUuid id = nsToUuid(itemId);
    const SecretStore::Kind k = toKind(kind);

    QObject::connect(
        m_store, &SecretStore::deleteFinished, m_store,
        [completion, id, k](const QUuid &connectionId, SecretStore::Kind finishedKind, bool ok,
                            const QString &error) {
            if (connectionId != id || finishedKind != k) {
                return;
            }
            NSString *err = qToNS(error);
            dispatchMain(^{
                if (completion) {
                    completion(ok, err);
                }
            });
        },
        Qt::SingleShotConnection);

    m_store->deleteSecret(id, k);
}

- (void)deleteAllSecretsForConnectionId:(NSUUID *)connectionId
                             completion:(ESSSecretBoolCompletion)completion
{
    // Core fires four deleteFinished signals; report overall success when all complete.
    // Does not delete TunnelSocksPassword (keyed by tunnel id).
    __block int remaining = 4;
    __block BOOL allOk = YES;
    __block NSString *lastError = @"";

    auto handleOne = ^(BOOL ok, NSString *error) {
        if (!ok) {
            allOk = NO;
            lastError = error ?: @"";
        }
        remaining -= 1;
        if (remaining == 0 && completion) {
            completion(allOk, lastError);
        }
    };

    [self deleteSecretForId:connectionId kind:ESSSecretKindPassword completion:handleOne];
    [self deleteSecretForId:connectionId
                       kind:ESSSecretKindPassphrase
                 completion:handleOne];
    [self deleteSecretForId:connectionId
                       kind:ESSSecretKindGatewayPassword
                 completion:handleOne];
    [self deleteSecretForId:connectionId
                       kind:ESSSecretKindGatewayPassphrase
                 completion:handleOne];
}

- (void)copySecretFromId:(NSUUID *)fromId
                    toId:(NSUUID *)toId
                    kind:(ESSSecretKind)kind
              completion:(ESSSecretBoolCompletion)completion
{
    const QUuid from = nsToUuid(fromId);
    const QUuid to = nsToUuid(toId);
    const SecretStore::Kind k = toKind(kind);

    QObject::connect(
        m_store, &SecretStore::storeFinished, m_store,
        [completion, to, k](const QUuid &connectionId, SecretStore::Kind finishedKind, bool ok,
                            const QString &error) {
            if (connectionId != to || finishedKind != k) {
                return;
            }
            NSString *err = qToNS(error);
            dispatchMain(^{
                if (completion) {
                    completion(ok, err);
                }
            });
        },
        Qt::SingleShotConnection);

    m_store->copySecret(from, to, k);
}

@end
