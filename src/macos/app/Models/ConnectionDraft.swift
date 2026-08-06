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

    /// Mirror core `ConnectionQuery::draftFromQuery` for palette / Quick Connect create.
    static func draftFromQuery(_ query: String) -> ConnectionDraft {
        var draft = ConnectionDraft()
        let trimmed = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return draft }

        var user = ""
        var hostPort = trimmed
        if let at = trimmed.firstIndex(of: "@") {
            user = String(trimmed[..<at]).trimmingCharacters(in: .whitespacesAndNewlines)
            hostPort = String(trimmed[trimmed.index(after: at)...]).trimmingCharacters(in: .whitespacesAndNewlines)
        }

        if let parsed = parseHostPort(hostPort) {
            draft.host = parsed.host
            draft.port = parsed.port
            if !user.isEmpty {
                draft.username = user
            }
            if !user.isEmpty {
                draft.name = user + "@" + parsed.host + (parsed.port == 22 ? "" : ":\(parsed.port)")
            } else {
                draft.name = parsed.host + (parsed.port == 22 ? "" : ":\(parsed.port)")
            }
        } else {
            draft.name = trimmed
            draft.host = trimmed
        }
        return draft
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

    private static func parseHostPort(_ text: String) -> (host: String, port: Int)? {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return nil }

        if trimmed.hasPrefix("[") {
            guard let close = trimmed.firstIndex(of: "]"), close > trimmed.startIndex else { return nil }
            let host = String(trimmed[trimmed.index(after: trimmed.startIndex)..<close])
            var port = 22
            let after = trimmed[trimmed.index(after: close)...]
            if after.hasPrefix(":") {
                guard let p = Int(after.dropFirst()), p >= 1, p <= 65535 else { return nil }
                port = p
            }
            return host.isEmpty ? nil : (host, port)
        }

        if let colon = trimmed.lastIndex(of: ":") {
            let hostPart = String(trimmed[..<colon])
            let portPart = String(trimmed[trimmed.index(after: colon)...])
            if !hostPart.contains(":"), let port = Int(portPart), port >= 1, port <= 65535 {
                return (hostPart, port)
            }
        }

        return (trimmed, 22)
    }
}
