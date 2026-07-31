// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ShellCommandSet.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QRegularExpression>

namespace
{
QString trCmd(const char *text)
{
    return QCoreApplication::translate("ShellCommandSet", text);
}

const QStringList &nationalVars()
{
    static const QStringList vars = {
        QStringLiteral("LANG"),
        QStringLiteral("LANGUAGE"),
        QStringLiteral("LC_CTYPE"),
        QStringLiteral("LC_COLLATE"),
        QStringLiteral("LC_MONETARY"),
        QStringLiteral("LC_NUMERIC"),
        QStringLiteral("LC_TIME"),
        QStringLiteral("LC_MESSAGES"),
        QStringLiteral("LC_ALL"),
        QStringLiteral("HUMAN_BLOCKS"),
        QStringLiteral("BLOCK_SIZE"),
        QStringLiteral("LS_BLOCK_SIZE"),
    };
    return vars;
}

const QStringList &aliasedCommands()
{
    static const QStringList cmds = {
        QStringLiteral("ls"),
        QStringLiteral("mkdir"),
        QStringLiteral("rm"),
        QStringLiteral("mv"),
        QStringLiteral("pwd"),
        QStringLiteral("realpath"),
        QStringLiteral("cd"),
        QStringLiteral("scp"),
        QStringLiteral("unalias"),
        QStringLiteral("unset"),
        QStringLiteral("echo"),
    };
    return cmds;
}

QString stripAnsi(QString text)
{
    static const QRegularExpression ansi(QStringLiteral("\\x1B\\[[0-9;]*[A-Za-z]"));
    return text.replace(ansi, QString());
}

bool isLsTypeChar(QChar typeChar)
{
    return typeChar == QLatin1Char('-') || typeChar == QLatin1Char('d') ||
           typeChar == QLatin1Char('l') || typeChar == QLatin1Char('c') ||
           typeChar == QLatin1Char('b') || typeChar == QLatin1Char('p') ||
           typeChar == QLatin1Char('s');
}

qint64 parseMtime(const QStringList &tokens, int *nameIndex)
{
    // Classic: Mon DD HH:MM name  OR  Mon DD YYYY name  OR full-time ISO-ish
    if (tokens.size() < 8) {
        return 0;
    }

    // Try --full-time style: YYYY-MM-DD HH:MM:SS.nnnnnnnnn +ZZZZ
    if (tokens.size() >= 9) {
        const QString &date = tokens.at(5);
        const QString &time = tokens.at(6);
        if (date.contains(QLatin1Char('-')) && date.size() >= 10) {
            QString stamp = date + QLatin1Char(' ') + time.left(8);
            const QDateTime dt =
                QDateTime::fromString(stamp, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            if (dt.isValid()) {
                *nameIndex = 8;
                if (tokens.size() > 8 && (tokens.at(7).startsWith(QLatin1Char('+')) ||
                                          tokens.at(7).startsWith(QLatin1Char('-')) ||
                                          tokens.at(7) == QLatin1String("UTC"))) {
                    *nameIndex = 8;
                } else {
                    *nameIndex = 7;
                }
                // Recalculate name index: date time [tz] name...
                if (tokens.size() > 7 && (tokens.at(7).startsWith(QLatin1Char('+')) ||
                                          tokens.at(7).startsWith(QLatin1Char('-')))) {
                    *nameIndex = 8;
                } else {
                    *nameIndex = 7;
                }
                return dt.toSecsSinceEpoch();
            }
        }
    }

    const QString &month = tokens.at(5);
    const QString &day = tokens.at(6);
    const QString &yearOrTime = tokens.at(7);
    *nameIndex = 8;

    QString stamp;
    if (yearOrTime.contains(QLatin1Char(':'))) {
        const int year = QDate::currentDate().year();
        stamp = QStringLiteral("%1 %2 %3 %4").arg(month, day).arg(year).arg(yearOrTime);
        QDateTime dt = QDateTime::fromString(stamp, QStringLiteral("MMM d yyyy HH:mm"));
        if (!dt.isValid()) {
            dt = QDateTime::fromString(stamp, QStringLiteral("MMM dd yyyy HH:mm"));
        }
        if (dt.isValid()) {
            if (dt > QDateTime::currentDateTime().addMonths(1)) {
                dt = dt.addYears(-1);
            }
            return dt.toSecsSinceEpoch();
        }
    } else {
        stamp = QStringLiteral("%1 %2 %3 00:00").arg(month, day, yearOrTime);
        QDateTime dt = QDateTime::fromString(stamp, QStringLiteral("MMM d yyyy HH:mm"));
        if (!dt.isValid()) {
            dt = QDateTime::fromString(stamp, QStringLiteral("MMM dd yyyy HH:mm"));
        }
        if (dt.isValid()) {
            return dt.toSecsSinceEpoch();
        }
    }
    return 0;
}

bool parseOneLsLine(const QString &lineIn, RemoteEntry *out, const QString &parentPath)
{
    QString line = stripAnsi(lineIn).trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1String("total "), Qt::CaseInsensitive)) {
        return false;
    }

    const QStringList tokens =
        line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (tokens.size() < 8) {
        return false;
    }

    const QString &perms = tokens.at(0);
    if (perms.size() < 10) {
        return false;
    }

    if (!isLsTypeChar(perms.at(0))) {
        return false;
    }
    const bool isDir = perms.at(0) == QLatin1Char('d');

    bool okSize = false;
    const qint64 size = tokens.at(4).toLongLong(&okSize);
    if (!okSize) {
        return false;
    }

    int nameIndex = 8;
    const qint64 mtime = parseMtime(tokens, &nameIndex);
    if (nameIndex >= tokens.size()) {
        return false;
    }

    QString name = tokens.mid(nameIndex).join(QLatin1Char(' '));
    // Symlink: "name -> target"
    const qsizetype arrow = name.indexOf(QStringLiteral(" -> "));
    if (arrow >= 0) {
        name = name.left(arrow);
    }
    if (name == QLatin1String(".") || name == QLatin1String("..")) {
        return false;
    }

    out->name = name;
    if (parentPath.endsWith(QLatin1Char('/'))) {
        out->path = parentPath + name;
    } else if (parentPath.isEmpty()) {
        out->path = name;
    } else {
        out->path = parentPath + QLatin1Char('/') + name;
    }
    out->isDir = isDir;
    out->size = size;
    out->permissions = perms;
    out->mtime = mtime;
    return true;
}
} // namespace

ShellCommandSet::ShellCommandSet(const ShellCommandSetConfig &config) : m_config(config) {}

void ShellCommandSet::setConfig(const ShellCommandSetConfig &config)
{
    m_config = config;
}

QString ShellCommandSet::shellQuote(const QString &arg)
{
    // POSIX single-quote escaping: ' -> '\''
    QString escaped = arg;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

QString ShellCommandSet::resolve(const QString &configured, const QString &fallback) const
{
    return configured.trimmed().isEmpty() ? fallback : configured.trimmed();
}

QString ShellCommandSet::applyTemplate(const QString &tmpl, const QStringList &args) const
{
    QString out = tmpl;
    for (int i = 0; i < args.size(); ++i) {
        out.replace(QLatin1Char('%') + QString::number(i + 1), args.at(i));
    }
    return out;
}

QString ShellCommandSet::listingBase() const
{
    return resolve(m_config.listingCommand, QStringLiteral("ls -la"));
}

QString ShellCommandSet::formatListDirectory(const QString &path, const QString &extraOptions) const
{
    return listingBase() +
           (extraOptions.trimmed().isEmpty() ? QString()
                                             : QLatin1Char(' ') + extraOptions.trimmed()) +
           QLatin1Char(' ') + shellQuote(path);
}

QString ShellCommandSet::formatListFile(const QString &path, const QString &extraOptions) const
{
    const QString custom = m_config.listFileCommand.trimmed();
    if (!custom.isEmpty()) {
        return applyTemplate(custom, {shellQuote(path)});
    }
    return listingBase() + QStringLiteral(" -d") +
           (extraOptions.trimmed().isEmpty() ? QString()
                                             : QLatin1Char(' ') + extraOptions.trimmed()) +
           QLatin1Char(' ') + shellQuote(path);
}

QString ShellCommandSet::formatMkdir(const QString &path) const
{
    const QString tmpl = resolve(m_config.mkdirCommand, QStringLiteral("mkdir %1"));
    return applyTemplate(tmpl, {shellQuote(path)});
}

QString ShellCommandSet::formatRemove(const QString &path) const
{
    const QString tmpl = resolve(m_config.removeCommand, QStringLiteral("rm -f -r %1"));
    return applyTemplate(tmpl, {shellQuote(path)});
}

QString ShellCommandSet::formatRename(const QString &from, const QString &to) const
{
    const QString tmpl = resolve(m_config.renameCommand, QStringLiteral("mv -f %1 %2"));
    return applyTemplate(tmpl, {shellQuote(from), shellQuote(to)});
}

QString ShellCommandSet::formatPwd() const
{
    return resolve(m_config.pwdCommand, QStringLiteral("pwd"));
}

QString ShellCommandSet::formatRealpath(const QString &path) const
{
    const QString tmpl = resolve(m_config.realpathCommand, QStringLiteral("realpath -e %1"));
    return applyTemplate(tmpl, {shellQuote(path)});
}

QString ShellCommandSet::formatStartupCommands() const
{
    QStringList parts;
    if (m_config.clearAliases) {
        for (const QString &cmd : aliasedCommands()) {
            parts.append(QStringLiteral("unalias %1 2>/dev/null").arg(cmd));
        }
    }
    if (m_config.clearNationalVars) {
        for (const QString &var : nationalVars()) {
            parts.append(QStringLiteral("unset %1 2>/dev/null").arg(var));
        }
    }
    if (parts.isEmpty()) {
        return {};
    }
    return parts.join(QStringLiteral("; "));
}

bool ShellCommandSet::parseLsListing(const QString &output,
                                     QVector<RemoteEntry> *outEntries,
                                     const QString &parentPath,
                                     QString *error)
{
    if (!outEntries) {
        if (error) {
            *error = trCmd("Internal error: missing entry buffer");
        }
        return false;
    }
    outEntries->clear();
    const QStringList lines = stripAnsi(output).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        RemoteEntry entry;
        if (parseOneLsLine(line, &entry, parentPath)) {
            outEntries->append(entry);
        }
    }
    return true;
}

bool ShellCommandSet::parseLsSingle(const QString &output,
                                    RemoteEntry *outEntry,
                                    const QString &path,
                                    QString *error)
{
    if (!outEntry) {
        if (error) {
            *error = trCmd("Internal error: missing entry");
        }
        return false;
    }

    QString parent = path;
    const qsizetype slash = parent.lastIndexOf(QLatin1Char('/'));
    QString expectedName;
    if (slash >= 0) {
        expectedName = parent.mid(slash + 1);
        parent = parent.left(slash);
        if (parent.isEmpty()) {
            parent = QStringLiteral("/");
        }
    } else {
        expectedName = path;
        parent.clear();
    }

    QVector<RemoteEntry> entries;
    if (!parseLsListing(output, &entries, parent, error)) {
        return false;
    }
    for (const RemoteEntry &e : entries) {
        if (e.name == expectedName || e.path == path) {
            *outEntry = e;
            outEntry->path = path;
            return true;
        }
    }
    // Fallback: first parsed line (ls -d on a path)
    if (!entries.isEmpty()) {
        *outEntry = entries.first();
        outEntry->path = path;
        if (outEntry->name.isEmpty()) {
            outEntry->name = expectedName;
        }
        return true;
    }
    if (error) {
        *error = trCmd("Cannot parse remote file listing");
    }
    return false;
}
