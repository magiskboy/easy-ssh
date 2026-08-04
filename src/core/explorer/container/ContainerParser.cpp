// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ContainerParser.h"

#include "core/explorer/container/backends/ContainerdBackend.h"
#include "core/explorer/container/backends/DockerBackend.h"
#include "core/explorer/container/backends/PodmanBackend.h"
#include "core/fs/ShellCommandSet.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QPair>
#include <memory>
#include <vector>

namespace ContainerParser
{
namespace
{
QString trParse(const char *text)
{
    return QCoreApplication::translate("ContainerParser", text);
}

bool looksLikeCommandMissing(const QString &stderrText, const QString &errorMessage)
{
    const QString hay = (stderrText + QLatin1Char('\n') + errorMessage).toLower();
    return hay.contains(QLatin1String("command not found")) ||
           hay.contains(QLatin1String("not found")) ||
           hay.contains(QLatin1String("no such file")) ||
           hay.contains(QLatin1String("executable file not found")) ||
           hay.contains(QLatin1String("no container runtime"));
}

bool looksLikePermissionDenied(const QString &stderrText, const QString &errorMessage)
{
    const QString hay = (stderrText + QLatin1Char('\n') + errorMessage).toLower();
    return hay.contains(QLatin1String("permission denied")) ||
           hay.contains(QLatin1String("operation not permitted")) ||
           hay.contains(QLatin1String("access denied")) ||
           hay.contains(QLatin1String("cannot connect to the docker daemon")) ||
           hay.contains(QLatin1String("error during connect"));
}

std::vector<std::unique_ptr<IContainerBackend>> makeBackends()
{
    std::vector<std::unique_ptr<IContainerBackend>> backends;
    backends.push_back(std::make_unique<PodmanBackend>());
    backends.push_back(std::make_unique<DockerBackend>());
    backends.push_back(std::make_unique<ContainerdBackend>());
    return backends;
}

IContainerBackend *
backendForRuntime(const QString &runtime,
                  const std::vector<std::unique_ptr<IContainerBackend>> &backends)
{
    for (const auto &backend : backends) {
        if (backend->runtimeId() == runtime) {
            return backend.get();
        }
    }
    return nullptr;
}
} // namespace

QString listCommand()
{
    const PodmanBackend podman;
    const DockerBackend docker;
    const ContainerdBackend containerd;

    // Single remote script: enable podman/docker when present; containerd only without docker.
    // After inventory, emit optional stats lines (kind=stats) for CPU/memory merge.
    return QStringLiteral("set +e\n"
                          "HAVE_PODMAN=0; HAVE_DOCKER=0; HAVE_CTR=0\n"
                          "command -v podman >/dev/null 2>&1 && HAVE_PODMAN=1\n"
                          "command -v docker >/dev/null 2>&1 && HAVE_DOCKER=1\n"
                          "command -v ctr >/dev/null 2>&1 && HAVE_CTR=1\n"
                          "USE_PODMAN=$HAVE_PODMAN\n"
                          "USE_DOCKER=$HAVE_DOCKER\n"
                          "USE_CONTAINERD=0\n"
                          "if [ \"$HAVE_CTR\" -eq 1 ] && [ \"$HAVE_DOCKER\" -eq 0 ]; then\n"
                          "  USE_CONTAINERD=1\n"
                          "fi\n"
                          "if [ \"$USE_PODMAN\" -eq 0 ] && [ \"$USE_DOCKER\" -eq 0 ] && "
                          "[ \"$USE_CONTAINERD\" -eq 0 ]; then\n"
                          "  echo 'no container runtime' >&2\n"
                          "  exit 127\n"
                          "fi\n"
                          "if [ \"$USE_PODMAN\" -eq 1 ]; then\n"
                          "  %1\n"
                          "  %4\n"
                          "fi\n"
                          "if [ \"$USE_DOCKER\" -eq 1 ]; then\n"
                          "  %2\n"
                          "  %5\n"
                          "fi\n"
                          "if [ \"$USE_CONTAINERD\" -eq 1 ]; then\n"
                          "  %3\n"
                          "fi\n"
                          "exit 0\n")
        .arg(podman.remoteListSnippet(),
             docker.remoteListSnippet(),
             containerd.remoteListSnippet(),
             podman.remoteStatsSnippet(),
             docker.remoteStatsSnippet());
}

namespace
{
struct StatsPatch
{
    double cpuPercent = -1.0;
    double memPercent = -1.0;
    QString memUsage;
};

bool idsMatch(const QString &fullId, const QString &statsId)
{
    if (fullId.isEmpty() || statsId.isEmpty()) {
        return false;
    }
    return fullId == statsId || fullId.startsWith(statsId) || statsId.startsWith(fullId.left(12));
}

void applyStatsPatch(ContainerInfo *info, const StatsPatch &patch)
{
    if (!info) {
        return;
    }
    if (patch.cpuPercent >= 0.0) {
        info->cpuPercent = patch.cpuPercent;
    }
    if (patch.memPercent >= 0.0) {
        info->memPercent = patch.memPercent;
    }
    if (!patch.memUsage.isEmpty()) {
        info->memUsage = patch.memUsage;
    }
}
} // namespace

bool parseList(const QByteArray &stdoutBytes, QVector<ContainerInfo> *out, QString *error)
{
    if (!out) {
        if (error) {
            *error = trParse("Output buffer is null");
        }
        return false;
    }
    out->clear();

    const auto backends = makeBackends();
    const QString text = QString::fromUtf8(stdoutBytes);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    out->reserve(lines.size());

    QVector<QPair<QString, StatsPatch>> stats; // runtime\nid → patch

    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || !line.startsWith(QLatin1Char('{'))) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }

        const QJsonObject object = doc.object();
        const QString runtime = object.value(QStringLiteral("runtime")).toString();
        if (object.value(QStringLiteral("kind")).toString() == QLatin1String("stats")) {
            StatsPatch patch;
            patch.cpuPercent = parsePercentValue(object.value(QStringLiteral("cpu")).toString());
            patch.memPercent =
                parsePercentValue(object.value(QStringLiteral("mem_percent")).toString());
            patch.memUsage = object.value(QStringLiteral("mem_usage")).toString().trimmed();
            if (patch.memUsage == QLatin1String("--") ||
                patch.memUsage == QLatin1String("-- / --")) {
                patch.memUsage.clear();
            }
            const QString statsId = object.value(QStringLiteral("id")).toString().trimmed();
            if (!runtime.isEmpty() && !statsId.isEmpty()) {
                stats.append({runtime + QLatin1Char('\n') + statsId, patch});
            }
            continue;
        }

        IContainerBackend *backend = backendForRuntime(runtime, backends);
        if (!backend) {
            continue;
        }

        ContainerInfo info;
        if (!backend->parseLine(object, &info)) {
            continue;
        }
        if (info.id().isEmpty()) {
            continue;
        }
        out->append(info);
    }

    for (ContainerInfo &info : *out) {
        for (const auto &entry : stats) {
            const int sep = entry.first.indexOf(QLatin1Char('\n'));
            if (sep < 0) {
                continue;
            }
            const QString runtime = entry.first.left(sep);
            const QString statsId = entry.first.mid(sep + 1);
            if (runtime != info.runtime) {
                continue;
            }
            if (idsMatch(info.containerId, statsId)) {
                applyStatsPatch(&info, entry.second);
                break;
            }
        }
    }

    return true;
}

ExplorerCapability classifyFailure(int exitStatus,
                                   const QByteArray &stderrBytes,
                                   const QString &errorMessage,
                                   QString *messageOut)
{
    const QString stderrText = QString::fromUtf8(stderrBytes).trimmed();
    const QString detail =
        !errorMessage.isEmpty()
            ? errorMessage
            : (!stderrText.isEmpty() ? stderrText
                                     : trParse("Remote command failed (exit %1)").arg(exitStatus));

    if (exitStatus == 127 || looksLikeCommandMissing(stderrText, errorMessage)) {
        if (messageOut) {
            *messageOut = trParse("No supported container runtime (podman, docker, or containerd) "
                                  "is available on this host.");
        }
        return ExplorerCapability::Unavailable;
    }
    if (looksLikePermissionDenied(stderrText, errorMessage) || exitStatus == 126) {
        if (messageOut) {
            *messageOut = trParse("Permission denied while listing containers.");
        }
        return ExplorerCapability::PermissionDenied;
    }
    if (messageOut) {
        *messageOut = detail;
    }
    return ExplorerCapability::Error;
}

QString normalizeState(const QString &rawState)
{
    const QString lower = rawState.trimmed().toLower();
    if (lower.isEmpty()) {
        return QStringLiteral("unknown");
    }
    if (lower == QLatin1String("running") || lower.startsWith(QLatin1String("up"))) {
        return QStringLiteral("running");
    }
    if (lower.contains(QLatin1String("exited")) || lower == QLatin1String("stopped") ||
        lower == QLatin1String("stop") || lower == QLatin1String("deleted") ||
        lower.contains(QLatin1String("dead")) || lower.contains(QLatin1String("removing"))) {
        return QStringLiteral("exited");
    }
    if (lower.contains(QLatin1String("created"))) {
        return QStringLiteral("created");
    }
    if (lower.contains(QLatin1String("paused")) || lower == QLatin1String("pausing")) {
        return QStringLiteral("paused");
    }
    if (lower.contains(QLatin1String("restarting"))) {
        return QStringLiteral("running");
    }
    return QStringLiteral("unknown");
}

QString formatStateDisplay(const QString &normalizedState)
{
    if (normalizedState == QLatin1String("running")) {
        return trParse("Running");
    }
    if (normalizedState == QLatin1String("exited")) {
        return trParse("Exited");
    }
    if (normalizedState == QLatin1String("created")) {
        return trParse("Created");
    }
    if (normalizedState == QLatin1String("paused")) {
        return trParse("Paused");
    }
    return trParse("Unknown");
}

QString shortId(const QString &containerId)
{
    if (containerId.size() <= 12) {
        return containerId;
    }
    return containerId.left(12);
}

QString displayName(const ContainerInfo &info)
{
    if (!info.name.isEmpty()) {
        return info.name;
    }
    return shortId(info.containerId);
}

QString formatOrDash(const QString &value)
{
    return value.trimmed().isEmpty() ? QStringLiteral("—") : value;
}

QString joinElidable(const QStringList &parts, const QString &sep)
{
    QStringList cleaned;
    cleaned.reserve(parts.size());
    for (const QString &part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            cleaned.append(trimmed);
        }
    }
    return cleaned.isEmpty() ? QString() : cleaned.join(sep);
}

double parsePercentValue(const QString &raw)
{
    QString text = raw.trimmed();
    if (text.isEmpty() || text == QLatin1String("--") || text == QLatin1String("N/A")) {
        return -1.0;
    }
    if (text.endsWith(QLatin1Char('%'))) {
        text.chop(1);
    }
    bool ok = false;
    const double value = text.toDouble(&ok);
    return ok ? value : -1.0;
}

QString formatCpuDisplay(double cpuPercent)
{
    if (cpuPercent < 0.0) {
        return QStringLiteral("—");
    }
    return QString::number(cpuPercent, 'f', cpuPercent >= 10.0 ? 1 : 2);
}

QString formatMemPercentDisplay(double memPercent)
{
    if (memPercent < 0.0) {
        return QStringLiteral("—");
    }
    return QString::number(memPercent, 'f', memPercent >= 10.0 ? 1 : 2);
}

QString inspectCommand(const ContainerInfo &info)
{
    if (info.containerId.isEmpty() || info.runtime.isEmpty()) {
        return {};
    }
    const QString id = ShellCommandSet::shellQuote(info.containerId);
    if (info.runtime == QLatin1String("podman")) {
        return QStringLiteral("podman container inspect --format json %1; "
                              "echo '__EASY_SSH_STATS__'; "
                              "podman stats --no-stream --format "
                              "'{\"cpu\":\"{{.CPU}}\",\"mem_percent\":\"{{.MemPerc}}\","
                              "\"mem_usage\":\"{{.MemUsage}}\"}' %1 2>/dev/null || true")
            .arg(id);
    }
    if (info.runtime == QLatin1String("docker")) {
        return QStringLiteral("docker inspect --type container %1; "
                              "echo '__EASY_SSH_STATS__'; "
                              "docker stats --no-stream --format "
                              "'{\"cpu\":\"{{.CPUPerc}}\",\"mem_percent\":\"{{.MemPerc}}\","
                              "\"mem_usage\":\"{{.MemUsage}}\"}' %1 2>/dev/null || true")
            .arg(id);
    }
    if (info.runtime == QLatin1String("containerd")) {
        const QString ns =
            info.runtimeNamespace.isEmpty() ? QStringLiteral("default") : info.runtimeNamespace;
        return QStringLiteral("ctr -n %1 containers info %2")
            .arg(ShellCommandSet::shellQuote(ns), id);
    }
    return {};
}

namespace
{
QString jsonString(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'f', 0);
    }
    return {};
}

QStringList jsonStringList(const QJsonValue &value)
{
    QStringList out;
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        out.reserve(array.size());
        for (const QJsonValue &item : array) {
            if (item.isString()) {
                out.append(item.toString());
            }
        }
    } else if (value.isString()) {
        out.append(value.toString());
    }
    return out;
}

QString joinJsonStringList(const QJsonValue &value)
{
    return joinElidable(jsonStringList(value), QStringLiteral(" "));
}

void fillFromDockerCompatible(const QJsonObject &root, ContainerInspectInfo *out)
{
    out->base.containerId = jsonString(root, "Id");
    out->base.name = jsonString(root, "Name");
    if (out->base.name.startsWith(QLatin1Char('/'))) {
        out->base.name = out->base.name.mid(1);
    }

    out->createdAt = jsonString(root, "Created");
    out->imageId = jsonString(root, "Image");
    out->imageName = jsonString(root, "ImageName");
    if (out->imageName.isEmpty()) {
        const QJsonObject config = root.value(QStringLiteral("Config")).toObject();
        out->imageName = jsonString(config, "Image");
    }
    out->base.image = out->imageName.isEmpty() ? out->imageId : out->imageName;
    out->driver = jsonString(root, "Driver");
    out->ociRuntime = jsonString(root, "OCIRuntime");
    if (out->ociRuntime.isEmpty()) {
        const QJsonObject hostConfig = root.value(QStringLiteral("HostConfig")).toObject();
        out->ociRuntime = jsonString(hostConfig, "Runtime");
    }
    out->restartCount = root.value(QStringLiteral("RestartCount")).toInt();

    const QJsonObject state = root.value(QStringLiteral("State")).toObject();
    if (!state.isEmpty()) {
        out->base.state = normalizeState(jsonString(state, "Status"));
        out->base.pid = static_cast<qint64>(state.value(QStringLiteral("Pid")).toDouble());
        out->exitCode = state.value(QStringLiteral("ExitCode")).toInt();
        out->oomKilled = state.value(QStringLiteral("OOMKilled")).toBool();
        out->stateError = jsonString(state, "Error");
        out->startedAt = jsonString(state, "StartedAt");
        out->finishedAt = jsonString(state, "FinishedAt");
    }

    const QJsonObject config = root.value(QStringLiteral("Config")).toObject();
    if (!config.isEmpty()) {
        out->hostname = jsonString(config, "Hostname");
        out->user = jsonString(config, "User");
        out->workingDir = jsonString(config, "WorkingDir");
        out->entrypoint = joinJsonStringList(config.value(QStringLiteral("Entrypoint")));
        out->command = joinJsonStringList(config.value(QStringLiteral("Cmd")));
        if (out->command.isEmpty()) {
            out->command = joinElidable(jsonStringList(root.value(QStringLiteral("Args"))),
                                        QStringLiteral(" "));
            const QString path = jsonString(root, "Path");
            if (!path.isEmpty()) {
                out->command =
                    out->command.isEmpty() ? path : (path + QLatin1Char(' ') + out->command);
            }
        }
        out->env = jsonStringList(config.value(QStringLiteral("Env")));

        const QJsonObject labels = config.value(QStringLiteral("Labels")).toObject();
        QStringList labelParts;
        for (auto it = labels.begin(); it != labels.end(); ++it) {
            labelParts.append(it.key() + QLatin1Char('=') + it.value().toString());
        }
        out->labels = joinElidable(labelParts);
    }

    const QJsonArray mounts = root.value(QStringLiteral("Mounts")).toArray();
    for (const QJsonValue &mountValue : mounts) {
        const QJsonObject mount = mountValue.toObject();
        const QString source = jsonString(mount, "Source");
        const QString destination = jsonString(mount, "Destination");
        if (destination.isEmpty() && jsonString(mount, "Target").isEmpty()) {
            continue;
        }
        const QString dest = destination.isEmpty() ? jsonString(mount, "Target") : destination;
        const bool rw = mount.contains(QStringLiteral("RW"))
                            ? mount.value(QStringLiteral("RW")).toBool()
                            : true;
        const QString value =
            QStringLiteral("%1 (%2)").arg(source.isEmpty() ? QStringLiteral("—") : source,
                                          rw ? QStringLiteral("rw") : QStringLiteral("ro"));
        out->mounts.append({dest, value});
    }

    const QJsonObject network = root.value(QStringLiteral("NetworkSettings")).toObject();
    if (!network.isEmpty()) {
        out->ipAddress = jsonString(network, "IPAddress");
        out->gateway = jsonString(network, "Gateway");
        out->macAddress = jsonString(network, "MacAddress");

        // Prefer Networks map when top-level IP is empty (compose/podman).
        if (out->ipAddress.isEmpty()) {
            const QJsonObject networks = network.value(QStringLiteral("Networks")).toObject();
            for (auto it = networks.begin(); it != networks.end(); ++it) {
                const QJsonObject endpoint = it.value().toObject();
                const QString ip = jsonString(endpoint, "IPAddress");
                if (!ip.isEmpty()) {
                    out->ipAddress = ip;
                    out->gateway = jsonString(endpoint, "Gateway");
                    out->macAddress = jsonString(endpoint, "MacAddress");
                    break;
                }
            }
        }

        const QJsonObject ports = network.value(QStringLiteral("Ports")).toObject();
        QStringList portParts;
        for (auto it = ports.begin(); it != ports.end(); ++it) {
            const QJsonArray bindings = it.value().toArray();
            if (bindings.isEmpty()) {
                portParts.append(it.key());
                continue;
            }
            for (const QJsonValue &bindingValue : bindings) {
                const QJsonObject binding = bindingValue.toObject();
                const QString hostIp = jsonString(binding, "HostIp");
                const QString hostPort = jsonString(binding, "HostPort");
                if (hostPort.isEmpty()) {
                    portParts.append(it.key());
                } else {
                    portParts.append(QStringLiteral("%1:%2→%3")
                                         .arg(hostIp.isEmpty() ? QStringLiteral("*") : hostIp,
                                              hostPort,
                                              it.key()));
                }
            }
        }
        out->ports = joinElidable(portParts);
    }
}

void fillFromContainerd(const QJsonObject &root, ContainerInspectInfo *out)
{
    out->base.containerId = jsonString(root, "ID");
    if (out->base.containerId.isEmpty()) {
        out->base.containerId = jsonString(root, "id");
    }
    out->base.image = jsonString(root, "Image");
    out->imageName = out->base.image;
    out->createdAt = jsonString(root, "CreatedAt");
    if (out->createdAt.isEmpty()) {
        out->createdAt = jsonString(root, "Created");
    }

    const QJsonObject runtime = root.value(QStringLiteral("Runtime")).toObject();
    out->ociRuntime = jsonString(runtime, "Name");

    const QJsonObject labels = root.value(QStringLiteral("Labels")).toObject();
    QStringList labelParts;
    for (auto it = labels.begin(); it != labels.end(); ++it) {
        labelParts.append(it.key() + QLatin1Char('=') + it.value().toString());
        if (it.key().endsWith(QLatin1String(".container.name")) ||
            it.key() == QLatin1String("name") || it.key().endsWith(QLatin1String("/name"))) {
            if (out->base.name.isEmpty()) {
                out->base.name = it.value().toString();
            }
        }
    }
    out->labels = joinElidable(labelParts);

    QJsonObject spec = root.value(QStringLiteral("Spec")).toObject();
    if (spec.isEmpty()) {
        spec = root.value(QStringLiteral("spec")).toObject();
    }
    if (!spec.isEmpty()) {
        out->hostname = jsonString(spec, "hostname");
        const QJsonObject process = spec.value(QStringLiteral("process")).toObject();
        out->user = jsonString(process.value(QStringLiteral("user")).toObject(), "username");
        if (out->user.isEmpty()) {
            const QJsonObject user = process.value(QStringLiteral("user")).toObject();
            const int uid = user.value(QStringLiteral("uid")).toInt(-1);
            if (uid >= 0) {
                out->user = QString::number(uid);
            }
        }
        out->workingDir = jsonString(process, "cwd");
        out->command = joinJsonStringList(process.value(QStringLiteral("args")));
        out->env = jsonStringList(process.value(QStringLiteral("env")));

        const QJsonArray mounts = spec.value(QStringLiteral("mounts")).toArray();
        for (const QJsonValue &mountValue : mounts) {
            const QJsonObject mount = mountValue.toObject();
            const QString source = jsonString(mount, "source");
            const QString destination = jsonString(mount, "destination");
            if (destination.isEmpty()) {
                continue;
            }
            const QJsonArray options = mount.value(QStringLiteral("options")).toArray();
            bool readOnly = false;
            for (const QJsonValue &opt : options) {
                if (opt.toString() == QLatin1String("ro")) {
                    readOnly = true;
                    break;
                }
            }
            out->mounts.append({destination,
                                QStringLiteral("%1 (%2)").arg(
                                    source.isEmpty() ? QStringLiteral("—") : source,
                                    readOnly ? QStringLiteral("ro") : QStringLiteral("rw"))});
        }
    }

    if (out->base.name.isEmpty()) {
        out->base.name = shortId(out->base.containerId);
    }
}
} // namespace

bool parseInspect(const QByteArray &stdoutBytes,
                  const ContainerInfo &seed,
                  ContainerInspectInfo *out,
                  QString *error)
{
    if (!out) {
        if (error) {
            *error = trParse("Output buffer is null");
        }
        return false;
    }

    *out = ContainerInspectInfo{};
    out->base = seed;

    QByteArray inspectBytes = stdoutBytes;
    QByteArray statsBytes;
    const QByteArray marker = QByteArrayLiteral("__EASY_SSH_STATS__");
    const int markerPos = stdoutBytes.indexOf(marker);
    if (markerPos >= 0) {
        inspectBytes = stdoutBytes.left(markerPos).trimmed();
        statsBytes = stdoutBytes.mid(markerPos + marker.size()).trimmed();
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(inspectBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) {
            *error = trParse("Failed to parse inspect JSON");
        }
        return false;
    }

    QJsonObject root;
    if (doc.isArray()) {
        const QJsonArray array = doc.array();
        if (array.isEmpty() || !array.first().isObject()) {
            if (error) {
                *error = trParse("Inspect returned an empty result");
            }
            return false;
        }
        root = array.first().toObject();
        fillFromDockerCompatible(root, out);
    } else if (doc.isObject()) {
        root = doc.object();
        if (seed.runtime == QLatin1String("containerd") || root.contains(QStringLiteral("Spec")) ||
            root.contains(QStringLiteral("spec")) || root.contains(QStringLiteral("Snapshotter"))) {
            fillFromContainerd(root, out);
        } else {
            fillFromDockerCompatible(root, out);
        }
    } else {
        if (error) {
            *error = trParse("Unexpected inspect JSON");
        }
        return false;
    }

    out->base.runtime = seed.runtime;
    if (out->base.containerId.isEmpty()) {
        out->base.containerId = seed.containerId;
    }
    if (out->base.name.isEmpty()) {
        out->base.name = seed.name;
    }
    if (out->base.image.isEmpty()) {
        out->base.image = seed.image;
    }
    if (out->base.state.isEmpty()) {
        out->base.state = seed.state;
    }
    if (out->base.pid <= 0) {
        out->base.pid = seed.pid;
    }
    out->base.runtimeNamespace = seed.runtimeNamespace;
    out->base.cpuPercent = seed.cpuPercent;
    out->base.memPercent = seed.memPercent;
    out->base.memUsage = seed.memUsage;
    if (out->imageName.isEmpty()) {
        out->imageName = out->base.image;
    }

    if (!statsBytes.isEmpty()) {
        const QJsonDocument statsDoc = QJsonDocument::fromJson(statsBytes);
        QJsonObject statsObject;
        if (statsDoc.isObject()) {
            statsObject = statsDoc.object();
        } else if (statsDoc.isArray() && !statsDoc.array().isEmpty()) {
            statsObject = statsDoc.array().first().toObject();
        } else {
            // NDJSON single line
            const QString line =
                QString::fromUtf8(statsBytes).split(QLatin1Char('\n'), Qt::SkipEmptyParts).value(0);
            const QJsonDocument lineDoc = QJsonDocument::fromJson(line.toUtf8());
            if (lineDoc.isObject()) {
                statsObject = lineDoc.object();
            }
        }
        if (!statsObject.isEmpty()) {
            const double cpu =
                parsePercentValue(statsObject.value(QStringLiteral("cpu")).toString());
            const double mem =
                parsePercentValue(statsObject.value(QStringLiteral("mem_percent")).toString());
            QString usage = statsObject.value(QStringLiteral("mem_usage")).toString().trimmed();
            if (usage == QLatin1String("--") || usage == QLatin1String("-- / --")) {
                usage.clear();
            }
            if (cpu >= 0.0) {
                out->base.cpuPercent = cpu;
            }
            if (mem >= 0.0) {
                out->base.memPercent = mem;
            }
            if (!usage.isEmpty()) {
                out->base.memUsage = usage;
            }
        }
    }

    return true;
}

} // namespace ContainerParser
