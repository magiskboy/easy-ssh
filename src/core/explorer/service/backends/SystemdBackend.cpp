// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SystemdBackend.h"

#include "core/explorer/service/ServiceParser.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

QString SystemdBackend::managerId() const
{
    return QStringLiteral("systemd");
}

QString SystemdBackend::remoteListSnippet() const
{
    // Probe JSON (--output=json, then --json=short). Fall back to plain list-units + awk NDJSON.
    // Optional unit-files JSON blob is separated by __EASY_SSH_UNIT_FILES__.
    // Avoid bash-only `local` — remote shell may be dash/POSIX sh.
    return QStringLiteral(
        "UNITS_JSON=\"\"\n"
        "FILES_JSON=\"\"\n"
        "try_json_units() {\n"
        "  _out=$(systemctl list-units --type=service --all --output=json --no-pager 2>/dev/null) "
        "|| "
        "return 1\n"
        "  case \"$_out\" in\n"
        "    \\[*|\\{*) UNITS_JSON=\"$_out\"; return 0 ;;\n"
        "  esac\n"
        "  return 1\n"
        "}\n"
        "try_json_units_alt() {\n"
        "  _out=$(systemctl list-units --type=service --all --json=short --no-pager 2>/dev/null) "
        "|| "
        "return 1\n"
        "  case \"$_out\" in\n"
        "    \\[*|\\{*) UNITS_JSON=\"$_out\"; return 0 ;;\n"
        "  esac\n"
        "  return 1\n"
        "}\n"
        "if try_json_units || try_json_units_alt; then\n"
        "  FILES_JSON=$(systemctl list-unit-files --type=service --output=json --no-pager "
        "2>/dev/null) || true\n"
        "  case \"$FILES_JSON\" in\n"
        "    \\[*|\\{*) ;;\n"
        "    *)\n"
        "      FILES_JSON=$(systemctl list-unit-files --type=service --json=short --no-pager "
        "2>/dev/null) || true\n"
        "      case \"$FILES_JSON\" in\n"
        "        \\[*|\\{*) ;;\n"
        "        *) FILES_JSON=\"\" ;;\n"
        "      esac\n"
        "      ;;\n"
        "  esac\n"
        "  printf '%s\\n' \"$UNITS_JSON\"\n"
        "  if [ -n \"$FILES_JSON\" ]; then\n"
        "    printf '%s\\n' '__EASY_SSH_UNIT_FILES__'\n"
        "    printf '%s\\n' \"$FILES_JSON\"\n"
        "  fi\n"
        "else\n"
        "  UNIT_FILES=$(systemctl list-unit-files --type=service --plain --no-legend --no-pager "
        "2>/dev/null || true)\n"
        "  systemctl list-units --type=service --all --plain --no-legend --no-pager 2>/dev/null | "
        "awk -v files=\"$UNIT_FILES\" '\n"
        "    BEGIN {\n"
        "      n = split(files, lines, \"\\n\")\n"
        "      for (i = 1; i <= n; i++) {\n"
        "        line = lines[i]\n"
        "        gsub(/^[ \\t]+|[ \\t]+$/, \"\", line)\n"
        "        if (line == \"\") continue\n"
        "        split(line, a, /[ \\t]+/)\n"
        "        if (a[1] != \"\") file_state[a[1]] = a[2]\n"
        "      }\n"
        "    }\n"
        "    {\n"
        "      unit = $1\n"
        "      load = $2\n"
        "      active = $3\n"
        "      substate = $4\n"
        "      desc = \"\"\n"
        "      for (i = 5; i <= NF; i++) {\n"
        "        if (i > 5) desc = desc \" \"\n"
        "        desc = desc $i\n"
        "      }\n"
        "      if (unit == \"\") next\n"
        "      gsub(/\\\\/, \"\\\\\\\\\", unit)\n"
        "      gsub(/\"/, \"\\\\\\\"\", unit)\n"
        "      gsub(/\\\\/, \"\\\\\\\\\", load)\n"
        "      gsub(/\"/, \"\\\\\\\"\", load)\n"
        "      gsub(/\\\\/, \"\\\\\\\\\", active)\n"
        "      gsub(/\"/, \"\\\\\\\"\", active)\n"
        "      gsub(/\\\\/, \"\\\\\\\\\", substate)\n"
        "      gsub(/\"/, \"\\\\\\\"\", substate)\n"
        "      gsub(/\\\\/, \"\\\\\\\\\", desc)\n"
        "      gsub(/\"/, \"\\\\\\\"\", desc)\n"
        "      ufs = file_state[unit]\n"
        "      gsub(/\\\\/, \"\\\\\\\\\", ufs)\n"
        "      gsub(/\"/, \"\\\\\\\"\", ufs)\n"
        "      printf "
        "\"{\\\"manager\\\":\\\"systemd\\\",\\\"unit\\\":\\\"%s\\\",\\\"load_state\\\":\\\"%s\\\","
        "\\\"active_state\\\":\\\"%s\\\",\\\"sub_state\\\":\\\"%s\\\",\\\"description\\\":\\\"%"
        "s\\\","
        "\\\"unit_file_state\\\":\\\"%s\\\",\\\"main_pid\\\":0}\\n\",\n"
        "             unit, load, active, substate, desc, ufs\n"
        "    }\n"
        "  '\n"
        "fi\n");
}

bool SystemdBackend::parseLine(const QJsonObject &object, ServiceInfo *out) const
{
    if (!out) {
        return false;
    }
    const QString manager = object.value(QStringLiteral("manager")).toString();
    if (!manager.isEmpty() && manager != managerId()) {
        return false;
    }

    QString unit = object.value(QStringLiteral("unit")).toString().trimmed();
    if (unit.isEmpty()) {
        unit = object.value(QStringLiteral("unit_file")).toString().trimmed();
    }
    if (unit.isEmpty()) {
        return false;
    }

    out->manager = managerId();
    out->unit = unit;
    out->description = object.value(QStringLiteral("description")).toString().trimmed();
    out->loadState = object.value(QStringLiteral("load_state")).toString().trimmed();
    if (out->loadState.isEmpty()) {
        out->loadState = object.value(QStringLiteral("load")).toString().trimmed();
    }
    const QString rawActive = object.value(QStringLiteral("active_state")).toString().trimmed();
    const QString activeAlt = object.value(QStringLiteral("active")).toString().trimmed();
    out->activeState =
        ServiceParser::normalizeActiveState(rawActive.isEmpty() ? activeAlt : rawActive);
    out->subState = object.value(QStringLiteral("sub_state")).toString().trimmed();
    if (out->subState.isEmpty()) {
        out->subState = object.value(QStringLiteral("sub")).toString().trimmed();
    }
    out->unitFileState = object.value(QStringLiteral("unit_file_state")).toString().trimmed();
    if (out->unitFileState.isEmpty()) {
        out->unitFileState = object.value(QStringLiteral("state")).toString().trimmed();
    }
    const QJsonValue pidValue = object.value(QStringLiteral("main_pid"));
    if (pidValue.isDouble()) {
        out->mainPid = static_cast<qint64>(pidValue.toDouble());
    } else if (pidValue.isString()) {
        bool ok = false;
        const qint64 pid = pidValue.toString().toLongLong(&ok);
        out->mainPid = ok ? pid : 0;
    } else {
        out->mainPid = 0;
    }
    return true;
}

bool SystemdBackend::parseJsonInventory(const QByteArray &stdoutBytes,
                                        QVector<ServiceInfo> *out,
                                        QString *error) const
{
    if (!out) {
        if (error) {
            *error = QStringLiteral("Output buffer is null");
        }
        return false;
    }
    out->clear();

    QByteArray unitsBytes = stdoutBytes.trimmed();
    QByteArray filesBytes;
    const QByteArray marker = QByteArrayLiteral("__EASY_SSH_UNIT_FILES__");
    const int markerPos = stdoutBytes.indexOf(marker);
    if (markerPos >= 0) {
        unitsBytes = stdoutBytes.left(markerPos).trimmed();
        filesBytes = stdoutBytes.mid(markerPos + marker.size()).trimmed();
    }

    QJsonParseError parseError;
    const QJsonDocument unitsDoc = QJsonDocument::fromJson(unitsBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !unitsDoc.isArray()) {
        if (error) {
            *error = QStringLiteral("Failed to parse systemd unit JSON");
        }
        return false;
    }

    QHash<QString, QString> fileStates;
    if (!filesBytes.isEmpty()) {
        const QJsonDocument filesDoc = QJsonDocument::fromJson(filesBytes);
        if (filesDoc.isArray()) {
            const QJsonArray files = filesDoc.array();
            for (const QJsonValue &value : files) {
                const QJsonObject object = value.toObject();
                const QString unitFile =
                    object.value(QStringLiteral("unit_file")).toString().trimmed();
                const QString state = object.value(QStringLiteral("state")).toString().trimmed();
                if (!unitFile.isEmpty() && !state.isEmpty()) {
                    fileStates.insert(unitFile, state);
                }
            }
        }
    }

    const QJsonArray units = unitsDoc.array();
    out->reserve(units.size());
    for (const QJsonValue &value : units) {
        if (!value.isObject()) {
            continue;
        }
        QJsonObject object = value.toObject();
        object.insert(QStringLiteral("manager"), managerId());
        if (!object.contains(QStringLiteral("load_state")) &&
            object.contains(QStringLiteral("load"))) {
            object.insert(QStringLiteral("load_state"), object.value(QStringLiteral("load")));
        }
        if (!object.contains(QStringLiteral("active_state")) &&
            object.contains(QStringLiteral("active"))) {
            object.insert(QStringLiteral("active_state"), object.value(QStringLiteral("active")));
        }
        if (!object.contains(QStringLiteral("sub_state")) &&
            object.contains(QStringLiteral("sub"))) {
            object.insert(QStringLiteral("sub_state"), object.value(QStringLiteral("sub")));
        }

        ServiceInfo info;
        if (!parseLine(object, &info)) {
            continue;
        }
        if (info.unitFileState.isEmpty()) {
            info.unitFileState = fileStates.value(info.unit);
        }
        if (info.id().isEmpty()) {
            continue;
        }
        out->append(info);
    }
    return true;
}
