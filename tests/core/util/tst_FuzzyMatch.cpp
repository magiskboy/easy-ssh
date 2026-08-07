// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/util/FuzzyMatch.h"

#include <QtTest>

class FuzzyMatchTest final : public QObject
{
    Q_OBJECT

private slots:
    void emptyPatternMatches();
    void caseInsensitiveAndSeparatorBonus();
    void unmatchedAndBestScore();
};

void FuzzyMatchTest::emptyPatternMatches()
{
    QCOMPARE(FuzzyMatch::matchScore({QString(), QStringLiteral("anything")}).value(), 0);
    QCOMPARE(
        FuzzyMatch::bestMatchScore(QStringLiteral("  "), {QStringLiteral("a"), QStringLiteral("b")})
            .value(),
        0);
}

void FuzzyMatchTest::caseInsensitiveAndSeparatorBonus()
{
    const auto plain = FuzzyMatch::matchScore(
        {.pattern = QStringLiteral("ssh"), .haystack = QStringLiteral("easySSH")});
    QVERIFY(plain.has_value());

    const auto separator = FuzzyMatch::matchScore(
        {.pattern = QStringLiteral("ssh"), .haystack = QStringLiteral("easy-ssh")});
    QVERIFY(separator.has_value());
    QVERIFY(*separator > *plain);

    const auto shortHay =
        FuzzyMatch::matchScore({.pattern = QStringLiteral("ab"), .haystack = QStringLiteral("ab")});
    const auto longHay = FuzzyMatch::matchScore(
        {.pattern = QStringLiteral("ab"), .haystack = QStringLiteral("xxaayybbzz")});
    QVERIFY(shortHay.has_value());
    QVERIFY(longHay.has_value());
    QVERIFY(*shortHay > *longHay);
}

void FuzzyMatchTest::unmatchedAndBestScore()
{
    QVERIFY(!FuzzyMatch::matchScore(
                 {.pattern = QStringLiteral("zzz"), .haystack = QStringLiteral("easy-ssh")})
                 .has_value());

    const auto best = FuzzyMatch::bestMatchScore(QStringLiteral("lab"),
                                                 {QString(),
                                                  QStringLiteral("production"),
                                                  QStringLiteral("lab-bastion"),
                                                  QStringLiteral("lab")});
    QVERIFY(best.has_value());
    const auto exact = FuzzyMatch::matchScore(
        {.pattern = QStringLiteral("lab"), .haystack = QStringLiteral("lab")});
    QCOMPARE(*best, *exact);
}

QTEST_GUILESS_MAIN(FuzzyMatchTest)

#include "tst_FuzzyMatch.moc"
