// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

enum FuzzyMatch {
    /// Case-insensitive subsequence match with a simple score (higher is better).
    /// Empty pattern matches everything with score 0.
    static func matchScore(pattern: String, haystack: String) -> Int? {
        let needle = pattern.trimmingCharacters(in: .whitespacesAndNewlines)
        if needle.isEmpty {
            return 0
        }

        var score = 0
        var consecutive = 0
        var hayIndex = haystack.startIndex

        for nc in needle {
            var found = false
            while hayIndex < haystack.endIndex {
                let hc = haystack[hayIndex]
                hayIndex = haystack.index(after: hayIndex)
                if String(hc).caseInsensitiveCompare(String(nc)) == .orderedSame {
                    consecutive += 1
                    score += 10 + consecutive * 5
                    let prevIndex = haystack.index(before: hayIndex)
                    if prevIndex == haystack.startIndex {
                        score += 20
                    } else {
                        let before = haystack[haystack.index(before: prevIndex)]
                        if !before.isLetter && !before.isNumber {
                            score += 20
                        }
                    }
                    found = true
                    break
                }
                consecutive = 0
            }
            if !found {
                return nil
            }
        }

        score += max(0, 50 - haystack.count)
        return score
    }

    static func bestMatchScore(pattern: String, fields: [String]) -> Int? {
        let needle = pattern.trimmingCharacters(in: .whitespacesAndNewlines)
        if needle.isEmpty {
            return 0
        }

        var best: Int?
        for field in fields where !field.isEmpty {
            guard let score = matchScore(pattern: needle, haystack: field) else { continue }
            if best == nil || score > best! {
                best = score
            }
        }
        return best
    }
}
