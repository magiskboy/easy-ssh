/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef ESS_SECRET_STORE_H
#define ESS_SECRET_STORE_H

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ESSSecretKind) {
    ESSSecretKindPassword = 0,
    ESSSecretKindPassphrase = 1,
    ESSSecretKindGatewayPassword = 2,
    ESSSecretKindGatewayPassphrase = 3,
    /// Keyed by tunnel id (not connection id).
    ESSSecretKindTunnelSocksPassword = 4,
};

typedef void (^ESSSecretBoolCompletion)(BOOL ok, NSString *error);
typedef void (^ESSSecretReadCompletion)(NSString *_Nullable value, BOOL ok, NSString *error);

/// Async keychain bridge over core SecretStore. Completions run on the main queue.
@interface ESSSecretStore : NSObject

+ (ESSSecretStore *)sharedStore;

- (void)storeSecretForId:(NSUUID *)itemId
                    kind:(ESSSecretKind)kind
                   value:(NSString *)value
              completion:(nullable ESSSecretBoolCompletion)completion;

- (void)readSecretForId:(NSUUID *)itemId
                   kind:(ESSSecretKind)kind
             completion:(nullable ESSSecretReadCompletion)completion;

- (void)deleteSecretForId:(NSUUID *)itemId
                     kind:(ESSSecretKind)kind
               completion:(nullable ESSSecretBoolCompletion)completion;

/// Deletes Password / Passphrase / Gateway* for a connection id (not TunnelSocksPassword).
- (void)deleteAllSecretsForConnectionId:(NSUUID *)connectionId
                             completion:(nullable ESSSecretBoolCompletion)completion;

- (void)copySecretFromId:(NSUUID *)fromId
                    toId:(NSUUID *)toId
                    kind:(ESSSecretKind)kind
              completion:(nullable ESSSecretBoolCompletion)completion;

@end

NS_ASSUME_NONNULL_END

#endif
