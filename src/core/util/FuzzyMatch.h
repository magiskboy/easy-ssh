/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <optional>

namespace FuzzyMatch
{

/// Inputs for subsequence scoring (avoids adjacent same-type parameters).
struct ScoreArgs
{
    QString pattern;
    QString haystack;
};

/// Case-insensitive subsequence match with a simple score (higher is better).
/// Empty pattern matches everything with score 0.
inline std::optional<int> matchScore(const ScoreArgs &args)
{
    const QString needle = args.pattern.trimmed();
    if (needle.isEmpty()) {
        return 0;
    }

    const QString &hay = args.haystack;
    int score = 0;
    int consecutive = 0;
    int hayIndex = 0;

    for (const QChar nc : needle) {
        bool found = false;
        while (hayIndex < hay.size()) {
            const QChar hc = hay.at(hayIndex);
            ++hayIndex;
            if (hc.toCaseFolded() == nc.toCaseFolded()) {
                ++consecutive;
                score += 10 + consecutive * 5;
                // Bonus for match at start or after a separator.
                if (hayIndex == 1 || !hay.at(hayIndex - 2).isLetterOrNumber()) {
                    score += 20;
                }
                found = true;
                break;
            }
            consecutive = 0;
        }
        if (!found) {
            return std::nullopt;
        }
    }

    // Prefer shorter haystacks when scores are otherwise similar.
    score += qMax(0, 50 - hay.size());
    return score;
}

/// Best score across multiple haystack fields; nullopt if pattern matches none.
inline std::optional<int> bestMatchScore(const QString &pattern, const QStringList &fields)
{
    const QString needle = pattern.trimmed();
    if (needle.isEmpty()) {
        return 0;
    }

    std::optional<int> best;
    for (const QString &field : fields) {
        if (field.isEmpty()) {
            continue;
        }
        if (const std::optional<int> score =
                matchScore(ScoreArgs{.pattern = needle, .haystack = field})) {
            if (!best || *score > *best) {
                best = score;
            }
        }
    }
    return best;
}

} // namespace FuzzyMatch
