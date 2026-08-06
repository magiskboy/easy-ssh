// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

struct ConnectionDraft: Equatable {
    var host: String = ""
    var port: Int = 22
    var username: String = NSUserName()
    var usePrivateKey: Bool = false
    var password: String = ""
    var privateKeyPath: String = ""

    var displayName: String {
        let user = username.isEmpty ? "user" : username
        let h = host.isEmpty ? "host" : host
        return "\(user)@\(h):\(port)"
    }

    var isValid: Bool {
        !host.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
            !username.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
            port > 0 && port <= 65535 &&
            (usePrivateKey ? !privateKeyPath.isEmpty : true)
    }
}
