// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ServiceParser.h"

#include "core/explorer/service/backends/SystemdBackend.h"
#include "core/fs/ShellCommandSet.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QtGlobal>
#include <memory>
#include <vector>

namespace ServiceParser
{
namespace
{
QString trParse(const char *text)
{
    return QCoreApplication::translate("ServiceParser", text);
}

bool looksLikeCommandMissing(const QString &stderrText, const QString &errorMessage)
{
    const QString hay = (stderrText + QLatin1Char('\n') + errorMessage).toLower();
    return hay.contains(QLatin1String("command not found")) ||
           hay.contains(QLatin1String("not found")) ||
           hay.contains(QLatin1String("no such file")) ||
           hay.contains(QLatin1String("executable file not found")) ||
           hay.contains(QLatin1String("no service manager"));
}

bool looksLikePermissionDenied(const QString &stderrText, const QString &errorMessage)
{
    const QString hay = (stderrText + QLatin1Char('\n') + errorMessage).toLower();
    return hay.contains(QLatin1String("permission denied")) ||
           hay.contains(QLatin1String("operation not permitted")) ||
           hay.contains(QLatin1String("access denied")) ||
           hay.contains(QLatin1String("interactive authentication required")) ||
           hay.contains(QLatin1String("auth failure"));
}

std::vector<std::unique_ptr<IServiceBackend>> makeBackends()
{
    std::vector<std::unique_ptr<IServiceBackend>> backends;
    backends.push_back(std::make_unique<SystemdBackend>());
    return backends;
}

IServiceBackend *backendForManager(const QString &manager,
                                   const std::vector<std::unique_ptr<IServiceBackend>> &backends)
{
    for (const auto &backend : backends) {
        if (backend->managerId() == manager) {
            return backend.get();
        }
    }
    return nullptr;
}

bool looksLikeJsonDocument(const QByteArray &bytes)
{
    int i = 0;
    while (i < bytes.size() && (bytes.at(i) == ' ' || bytes.at(i) == '\n' || bytes.at(i) == '\r' ||
                                bytes.at(i) == '\t')) {
        ++i;
    }
    if (i >= bytes.size()) {
        return false;
    }
    const char c = bytes.at(i);
    return c == '[' || c == '{';
}

QHash<QString, QString> parseShowProperties(const QByteArray &bytes)
{
    QHash<QString, QString> props;
    const QString text = QString::fromUtf8(bytes);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        props.insert(line.left(eq), line.mid(eq + 1));
    }
    return props;
}
} // namespace

QString listCommand()
{
    const SystemdBackend systemd;

    return QStringLiteral("set +e\n"
                          "if ! command -v systemctl >/dev/null 2>&1; then\n"
                          "  echo 'no service manager' >&2\n"
                          "  exit 127\n"
                          "fi\n"
                          "%1\n"
                          "exit 0\n")
        .arg(systemd.remoteListSnippet());
}

bool parseList(const QByteArray &stdoutBytes, QVector<ServiceInfo> *out, QString *error)
{
    if (!out) {
        if (error) {
            *error = trParse("Output buffer is null");
        }
        return false;
    }
    out->clear();

    const QByteArray trimmed = stdoutBytes.trimmed();
    // systemctl --output=json emits a JSON array. The POSIX awk fallback emits NDJSON
    // objects (one {...} per line). Both start with '{' or '[', but only arrays belong
    // in parseJsonInventory — routing NDJSON there yields "garbage at the end".
    if (!trimmed.isEmpty() && trimmed.startsWith('[')) {
        SystemdBackend systemd;
        return systemd.parseJsonInventory(stdoutBytes, out, error);
    }

    const auto backends = makeBackends();
    const QString text = QString::fromUtf8(stdoutBytes);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    out->reserve(lines.size());

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
        const QString manager = object.value(QStringLiteral("manager")).toString();
        IServiceBackend *backend = backendForManager(manager, backends);
        if (!backend) {
            continue;
        }

        ServiceInfo info;
        if (!backend->parseLine(object, &info)) {
            continue;
        }
        if (info.id().isEmpty()) {
            continue;
        }
        out->append(info);
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
            *messageOut =
                trParse("No supported service manager (systemd) is available on this host.");
        }
        return ExplorerCapability::Unavailable;
    }
    if (looksLikePermissionDenied(stderrText, errorMessage) || exitStatus == 126) {
        if (messageOut) {
            *messageOut = trParse("Permission denied while listing services.");
        }
        return ExplorerCapability::PermissionDenied;
    }
    if (messageOut) {
        *messageOut = detail;
    }
    return ExplorerCapability::Error;
}

QString normalizeActiveState(const QString &rawState)
{
    const QString lower = rawState.trimmed().toLower();
    if (lower.isEmpty()) {
        return QStringLiteral("unknown");
    }
    if (lower == QLatin1String("active")) {
        return QStringLiteral("active");
    }
    if (lower == QLatin1String("inactive")) {
        return QStringLiteral("inactive");
    }
    if (lower == QLatin1String("failed")) {
        return QStringLiteral("failed");
    }
    if (lower == QLatin1String("activating")) {
        return QStringLiteral("activating");
    }
    if (lower == QLatin1String("deactivating")) {
        return QStringLiteral("deactivating");
    }
    if (lower == QLatin1String("reloading") || lower == QLatin1String("maintenance")) {
        return lower;
    }
    return QStringLiteral("unknown");
}

QString formatActiveStateDisplay(const QString &normalizedState)
{
    if (normalizedState == QLatin1String("active")) {
        return trParse("Active");
    }
    if (normalizedState == QLatin1String("inactive")) {
        return trParse("Inactive");
    }
    if (normalizedState == QLatin1String("failed")) {
        return trParse("Failed");
    }
    if (normalizedState == QLatin1String("activating")) {
        return trParse("Activating");
    }
    if (normalizedState == QLatin1String("deactivating")) {
        return trParse("Deactivating");
    }
    if (normalizedState == QLatin1String("reloading")) {
        return trParse("Reloading");
    }
    if (normalizedState == QLatin1String("maintenance")) {
        return trParse("Maintenance");
    }
    return trParse("Unknown");
}

QString formatOrDash(const QString &value)
{
    return value.trimmed().isEmpty() ? QStringLiteral("—") : value;
}

QString formatPidDisplay(qint64 pid)
{
    return pid > 0 ? QString::number(pid) : QStringLiteral("—");
}

QString inspectCommand(const ServiceInfo &info)
{
    if (info.unit.isEmpty()) {
        return {};
    }
    if (info.manager != QLatin1String("systemd") && !info.manager.isEmpty()) {
        return {};
    }

    const QString unit = ShellCommandSet::shellQuote(info.unit);
    return QStringLiteral("systemctl show %1 "
                          "-p Id -p Description -p LoadState -p ActiveState -p SubState "
                          "-p UnitFileState -p MainPID -p FragmentPath "
                          "-p ActiveEnterTimestamp -p ExecMainStartTimestamp "
                          "-p Type -p Restart -p RemainAfterExit "
                          "--no-pager")
        .arg(unit);
}

QString followLogsCommand(const ServiceInfo &info, int lines)
{
    if (info.unit.isEmpty()) {
        return {};
    }
    if (info.manager != QLatin1String("systemd") && !info.manager.isEmpty()) {
        return {};
    }

    const int safeLines = qMax(1, lines);
    const QString unit = ShellCommandSet::shellQuote(info.unit);
    return QStringLiteral("journalctl --no-pager -f -n %1 -u %2").arg(safeLines).arg(unit);
}

bool parseInspect(const QByteArray &stdoutBytes,
                  const ServiceInfo &seed,
                  ServiceInspectInfo *out,
                  QString *error)
{
    if (!out) {
        if (error) {
            *error = trParse("Output buffer is null");
        }
        return false;
    }

    *out = ServiceInspectInfo{};
    out->base = seed;

    // Prefer JSON object when present; otherwise key=value from systemctl show.
    if (looksLikeJsonDocument(stdoutBytes)) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(stdoutBytes.trimmed(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            const SystemdBackend systemd;
            QJsonObject object = doc.object();
            object.insert(QStringLiteral("manager"), QStringLiteral("systemd"));
            ServiceInfo parsed;
            if (systemd.parseLine(object, &parsed)) {
                out->base = parsed;
                if (out->base.manager.isEmpty()) {
                    out->base.manager =
                        seed.manager.isEmpty() ? QStringLiteral("systemd") : seed.manager;
                }
            }
            out->fragmentPath = object.value(QStringLiteral("FragmentPath")).toString();
            if (out->fragmentPath.isEmpty()) {
                out->fragmentPath = object.value(QStringLiteral("fragment_path")).toString();
            }
            out->activeEnterTimestamp =
                object.value(QStringLiteral("ActiveEnterTimestamp")).toString();
            out->execMainStartTimestamp =
                object.value(QStringLiteral("ExecMainStartTimestamp")).toString();
            out->type = object.value(QStringLiteral("Type")).toString();
            out->restart = object.value(QStringLiteral("Restart")).toString();
            const QJsonValue remain = object.value(QStringLiteral("RemainAfterExit"));
            if (remain.isBool()) {
                out->remainAfterExit = remain.toBool();
            } else {
                out->remainAfterExit =
                    remain.toString().compare(QLatin1String("yes"), Qt::CaseInsensitive) == 0;
            }
            if (out->base.unit.isEmpty()) {
                out->base.unit = seed.unit;
            }
            return true;
        }
    }

    const QHash<QString, QString> props = parseShowProperties(stdoutBytes);
    if (props.isEmpty()) {
        if (error) {
            *error = trParse("Failed to parse service inspect output");
        }
        return false;
    }

    const QString id = props.value(QStringLiteral("Id"));
    if (!id.isEmpty()) {
        out->base.unit = id;
    }
    out->base.manager = seed.manager.isEmpty() ? QStringLiteral("systemd") : seed.manager;
    out->base.description = props.value(QStringLiteral("Description"), seed.description);
    out->base.loadState = props.value(QStringLiteral("LoadState"), seed.loadState);
    out->base.activeState =
        normalizeActiveState(props.value(QStringLiteral("ActiveState"), seed.activeState));
    out->base.subState = props.value(QStringLiteral("SubState"), seed.subState);
    out->base.unitFileState = props.value(QStringLiteral("UnitFileState"), seed.unitFileState);
    bool ok = false;
    const qint64 pid = props.value(QStringLiteral("MainPID")).toLongLong(&ok);
    out->base.mainPid = ok ? pid : seed.mainPid;
    out->fragmentPath = props.value(QStringLiteral("FragmentPath"));
    out->activeEnterTimestamp = props.value(QStringLiteral("ActiveEnterTimestamp"));
    out->execMainStartTimestamp = props.value(QStringLiteral("ExecMainStartTimestamp"));
    out->type = props.value(QStringLiteral("Type"));
    out->restart = props.value(QStringLiteral("Restart"));
    out->remainAfterExit = props.value(QStringLiteral("RemainAfterExit"))
                               .compare(QLatin1String("yes"), Qt::CaseInsensitive) == 0;

    return true;
}

} // namespace ServiceParser
