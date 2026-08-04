// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ContainerdBackend.h"

#include "core/explorer/container/ContainerParser.h"

QString ContainerdBackend::runtimeId() const
{
    return QStringLiteral("containerd");
}

QString ContainerdBackend::remoteListSnippet() const
{
    // Walk namespaces; join containers + tasks via a temp task map file; emit NDJSON.
    return QStringLiteral(
        "ctr ns ls -q 2>/dev/null | while IFS= read -r ns; do\n"
        "  [ -n \"$ns\" ] || continue\n"
        "  tf=$(mktemp 2>/dev/null) || tf=\"/tmp/easy-ssh-ctr-tasks-$$\";\n"
        "  ctr -n \"$ns\" tasks ls 2>/dev/null | awk 'NR>1 {print $1\"\\t\"$2\"\\t\"tolower($3)}' "
        "> \"$tf\"\n"
        "  ctr -n \"$ns\" containers ls 2>/dev/null | awk -v ns=\"$ns\" -v tf=\"$tf\" '\n"
        "    BEGIN {\n"
        "      while ((getline line < tf) > 0) {\n"
        "        split(line, f, \"\\t\");\n"
        "        if (f[1] != \"\") { pid[f[1]] = f[2]; st[f[1]] = f[3]; }\n"
        "      }\n"
        "      close(tf);\n"
        "    }\n"
        "    NR == 1 { next }\n"
        "    NF < 1 { next }\n"
        "    {\n"
        "      id = $1;\n"
        "      image = (NF >= 2 ? $2 : \"\");\n"
        "      if (image == \"-\") image = \"\";\n"
        "      status = (id in st) ? st[id] : \"created\";\n"
        "      p = (id in pid) ? pid[id] + 0 : 0;\n"
        "      short = substr(id, 1, 12);\n"
        "      gsub(/\\\\/, \"\\\\\\\\\", image); gsub(/\"/, \"\\\\\\\"\", image);\n"
        "      gsub(/\\\\/, \"\\\\\\\\\", ns); gsub(/\"/, \"\\\\\\\"\", ns);\n"
        "      printf "
        "\"{\\\"runtime\\\":\\\"containerd\\\",\\\"id\\\":\\\"%s\\\",\\\"name\\\":\\\"%s\\\","
        "\\\"image\\\":\\\"%s\\\",\\\"state\\\":\\\"%s\\\",\\\"pid\\\":%d,\\\"namespace\\\":\\\"%"
        "s\\\"}\\n\",\n"
        "        id, short, image, status, p, ns;\n"
        "    }'\n"
        "  rm -f \"$tf\" 2>/dev/null\n"
        "done");
}

bool ContainerdBackend::parseLine(const QJsonObject &object, ContainerInfo *out) const
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
    out->runtimeNamespace = object.value(QStringLiteral("namespace")).toString().trimmed();
    if (out->name.isEmpty()) {
        out->name = ContainerParser::shortId(id);
    }
    return true;
}
