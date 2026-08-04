// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "PodmanBackend.h"

#include "core/explorer/container/ContainerParser.h"

QString PodmanBackend::runtimeId() const
{
    return QStringLiteral("podman");
}

QString PodmanBackend::remoteListSnippet() const
{
    // Normalized NDJSON via Go template. ps format does not reliably expose Pid.
    return QStringLiteral(
        "podman ps -a --no-trunc --format "
        "'{\"runtime\":\"podman\",\"id\":\"{{.ID}}\",\"name\":\"{{.Names}}\","
        "\"image\":\"{{.Image}}\",\"state\":\"{{.State}}\",\"pid\":0,\"namespace\":\"\"}' "
        "2>/dev/null");
}

QString PodmanBackend::remoteStatsSnippet() const
{
    return QStringLiteral(
        "podman stats --no-stream --format "
        "'{\"runtime\":\"podman\",\"kind\":\"stats\",\"id\":\"{{.ID}}\","
        "\"cpu\":\"{{.CPU}}\",\"mem_percent\":\"{{.MemPerc}}\",\"mem_usage\":\"{{.MemUsage}}\"}' "
        "2>/dev/null");
}

bool PodmanBackend::parseLine(const QJsonObject &object, ContainerInfo *out) const
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
    out->image = object.value(QStringLiteral("image")).toString().trimmed();
    out->state = ContainerParser::normalizeState(object.value(QStringLiteral("state")).toString());
    out->pid = static_cast<qint64>(object.value(QStringLiteral("pid")).toDouble());
    if (out->pid < 0) {
        out->pid = 0;
    }
    out->runtimeNamespace.clear();
    if (out->name.isEmpty()) {
        out->name = ContainerParser::shortId(id);
    }
    return true;
}
