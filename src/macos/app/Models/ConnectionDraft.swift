// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

struct ConnectionDraft: Equatable {
    var name: String = ""
    var host: String = ""
    var port: Int = 22
    var username: String = NSUserName()
    var usePrivateKey: Bool = false
    var password: String = ""
    var passphrase: String = ""
    var privateKeyPath: String = ""
    var saveConnection: Bool = false
    var savePassword: Bool = false

    var displayName: String {
        if !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            return name
        }
        let user = username.isEmpty ? "user" : username
        let h = host.isEmpty ? "host" : host
        return "\(user)@\(h):\(port)"
    }

    var isValid: Bool {
        !host.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
            !username.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
            port > 0 && port <= 65535 &&
            (usePrivateKey ? !privateKeyPath.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty : true)
    }

    /// Lightweight `user@host:port` / `host` parse for Quick Connect prefill.
    static func parseQuery(_ query: String) -> ConnectionDraft {
        var draft = ConnectionDraft()
        let trimmed = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return draft }

        var remainder = trimmed
        if let at = remainder.firstIndex(of: "@") {
            draft.username = String(remainder[..<at])
            remainder = String(remainder[remainder.index(after: at)...])
        }

        if remainder.hasPrefix("["), let close = remainder.firstIndex(of: "]") {
            draft.host = String(remainder[remainder.index(after: remainder.startIndex)..<close])
            let after = remainder[remainder.index(after: close)...]
            if after.hasPrefix(":"), let port = Int(after.dropFirst()) {
                draft.port = port
            }
        } else if let colon = remainder.lastIndex(of: ":"),
                  let port = Int(remainder[remainder.index(after: colon)...]),
                  port > 0, port <= 65535
        {
            draft.host = String(remainder[..<colon])
            draft.port = port
        } else {
            draft.host = remainder
        }
        return draft
    }
}
