/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "core/explorer/container/backends/ContainerdBackend.h"
#include "core/explorer/container/backends/DockerBackend.h"
#include "core/explorer/container/backends/PodmanBackend.h"

#include <QJsonObject>
#include <QtTest>

class ContainerBackendsTest final : public QObject
{
    Q_OBJECT

private slots:
    void runtimeIdsAndSnippets();
    void dockerParseLine();
    void podmanParseLine();
    void containerdParseLine();
};

void ContainerBackendsTest::runtimeIdsAndSnippets()
{
    const DockerBackend docker;
    QCOMPARE(docker.runtimeId(), QStringLiteral("docker"));
    QVERIFY(docker.remoteListSnippet().contains(QStringLiteral("docker ps")));
    QVERIFY(docker.remoteStatsSnippet().contains(QStringLiteral("kind\":\"stats")));

    const PodmanBackend podman;
    QCOMPARE(podman.runtimeId(), QStringLiteral("podman"));
    QVERIFY(podman.remoteListSnippet().contains(QStringLiteral("podman ps")));
    QVERIFY(podman.remoteStatsSnippet().contains(QStringLiteral("podman stats")));

    const ContainerdBackend containerd;
    QCOMPARE(containerd.runtimeId(), QStringLiteral("containerd"));
    QVERIFY(containerd.remoteListSnippet().contains(QStringLiteral("ctr ns ls")));
}

void ContainerBackendsTest::dockerParseLine()
{
    const DockerBackend docker;
    ContainerInfo info;
    QVERIFY(!docker.parseLine(QJsonObject{}, &info));

    QJsonObject wrongRuntime{{QStringLiteral("runtime"), QStringLiteral("podman")},
                             {QStringLiteral("id"), QStringLiteral("abc")}};
    QVERIFY(!docker.parseLine(wrongRuntime, &info));

    QJsonObject object{{QStringLiteral("runtime"), QStringLiteral("docker")},
                       {QStringLiteral("id"), QStringLiteral("1234567890abcdef")},
                       {QStringLiteral("name"), QStringLiteral("/web,alias")},
                       {QStringLiteral("image"), QStringLiteral("nginx:latest")},
                       {QStringLiteral("state"), QStringLiteral("Up 3 minutes")}};
    QVERIFY(docker.parseLine(object, &info));
    QCOMPARE(info.runtime, QStringLiteral("docker"));
    QCOMPARE(info.containerId, QStringLiteral("1234567890abcdef"));
    QCOMPARE(info.name, QStringLiteral("web"));
    QCOMPARE(info.image, QStringLiteral("nginx:latest"));
    QCOMPARE(info.state, QStringLiteral("running"));
    QCOMPARE(info.pid, 0LL);
}

void ContainerBackendsTest::podmanParseLine()
{
    const PodmanBackend podman;
    ContainerInfo info;
    QJsonObject object{{QStringLiteral("runtime"), QStringLiteral("podman")},
                       {QStringLiteral("id"), QStringLiteral("abcdef1234567890")},
                       {QStringLiteral("name"), QString()},
                       {QStringLiteral("image"), QStringLiteral("redis")},
                       {QStringLiteral("state"), QStringLiteral("created")},
                       {QStringLiteral("pid"), 321}};
    QVERIFY(podman.parseLine(object, &info));
    QCOMPARE(info.name, QStringLiteral("abcdef123456"));
    QCOMPARE(info.state, QStringLiteral("created"));
    QCOMPARE(info.pid, 321LL);
}

void ContainerBackendsTest::containerdParseLine()
{
    const ContainerdBackend containerd;
    ContainerInfo info;
    QJsonObject object{{QStringLiteral("runtime"), QStringLiteral("containerd")},
                       {QStringLiteral("id"), QStringLiteral("deadbeefcafebabe")},
                       {QStringLiteral("name"), QStringLiteral("short")},
                       {QStringLiteral("image"), QStringLiteral("busybox")},
                       {QStringLiteral("state"), QStringLiteral("running")},
                       {QStringLiteral("pid"), 99},
                       {QStringLiteral("namespace"), QStringLiteral("k8s.io")}};
    QVERIFY(containerd.parseLine(object, &info));
    QCOMPARE(info.runtimeNamespace, QStringLiteral("k8s.io"));
    QCOMPARE(info.pid, 99LL);
    QCOMPARE(info.state, QStringLiteral("running"));
}

QTEST_GUILESS_MAIN(ContainerBackendsTest)

#include "tst_ContainerBackends.moc"
