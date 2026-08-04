// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "DockerBackend.h"

#include "core/explorer/container/ContainerParser.h"

QString DockerBackend::runtimeId() const
{
    return QStringLiteral("docker");
}

QString DockerBackend::remoteListSnippet() const
{
    // docker ps does not expose PID in format templates; leave pid=0.
    return QStringLiteral(
        "docker ps -a --no-trunc --format "
        "'{\"runtime\":\"docker\",\"id\":\"{{.ID}}\",\"name\":\"{{.Names}}\","
        "\"image\":\"{{.Image}}\",\"state\":\"{{.State}}\",\"pid\":0,\"namespace\":\"\"}' "
        "2>/dev/null");
}

QString DockerBackend::remoteStatsSnippet() const
{
    return QStringLiteral("docker stats --no-stream --format "
                          "'{\"runtime\":\"docker\",\"kind\":\"stats\",\"id\":\"{{.ID}}\","
                          "\"cpu\":\"{{.CPUPerc}}\",\"mem_percent\":\"{{.MemPerc}}\","
                          "\"mem_usage\":\"{{.MemUsage}}\"}' "
                          "2>/dev/null");
}

bool DockerBackend::parseLine(const QJsonObject &object, ContainerInfo *out) const
{
    if (!out) {
        return false;
    }
    if (object.value(QStringLiteral("runtime")).toString() != runtimeId()) {
        return false;
    }
    const QString id = object.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty()) {
        return false;
    }
    out->runtime = runtimeId();
    out->containerId = id;
    out->name = object.value(QStringLiteral("name")).toString().trimmed();
    // Docker may return "/name" or comma-separated names.
    if (out->name.startsWith(QLatin1Char('/'))) {
        out->name = out->name.mid(1);
    }
    const int comma = out->name.indexOf(QLatin1Char(','));
    if (comma >= 0) {
        out->name = out->name.left(comma);
    }
    out->image = object.value(QStringLiteral("image")).toString().trimmed();
    out->state = ContainerParser::normalizeState(object.value(QStringLiteral("state")).toString());
    out->pid = 0;
    out->runtimeNamespace.clear();
    if (out->name.isEmpty()) {
        out->name = ContainerParser::shortId(id);
    }
    return true;
}
