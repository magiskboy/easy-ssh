/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QByteArray>
#include <QFont>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUuid>

class AppSettings final : public QObject
{
    Q_OBJECT

public:
    static AppSettings &instance();

    // --- File Explorer ---
    bool showSizeColumn() const;
    void setShowSizeColumn(bool value);

    bool showPermissionsColumn() const;
    void setShowPermissionsColumn(bool value);

    bool showModifiedColumn() const;
    void setShowModifiedColumn(bool value);

    bool showHiddenFiles() const;
    void setShowHiddenFiles(bool value);

    QString defaultDownloadDir() const;
    void setDefaultDownloadDir(const QString &path);

    // --- Recent connections ---
    QList<QUuid> recentConnectionIds(int limit = 8) const;
    void recordRecentConnection(const QUuid &id);

    // --- Terminal ---
    QFont terminalFont() const;
    void setTerminalFont(const QFont &font);

    QString colorScheme() const;
    void setColorScheme(const QString &scheme);

    int historySize() const;
    void setHistorySize(int lines);

    /// 0 = Block, 1 = Underline, 2 = IBeam
    int cursorShape() const;
    void setCursorShape(int shape);

    bool cursorBlink() const;
    void setCursorBlink(bool blink);

    bool confirmMultilinePaste() const;
    void setConfirmMultilinePaste(bool confirm);

    // --- Session / General ---
    bool autoReconnect() const;
    void setAutoReconnect(bool enabled);

    // --- Main window ---
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

    // --- Sidebar ---
    static constexpr int kSidebarMinWidth = 220;
    static constexpr int kSidebarMaxWidth = 420;
    static constexpr int kSidebarDefaultWidth = 260;

    int sidebarWidth() const;
    void setSidebarWidth(int width);
    int sidebarTabIndex() const;
    void setSidebarTabIndex(int index);

    // --- Shortcuts ---
    QKeySequence shortcut(const QString &actionId) const;
    void setShortcut(const QString &actionId, const QKeySequence &sequence);
    void resetShortcutsToDefaults();

    static QStringList shortcutActionIds();
    static QKeySequence defaultShortcut(const QString &actionId);
    static QString shortcutLabel(const QString &actionId);
    static QString shortcutGroup(const QString &actionId);

    void notifyChanged();

signals:
    void settingsChanged();

private:
    explicit AppSettings(QObject *parent = nullptr);

    bool boolValue(const QString &key, bool defaultValue) const;
    void setBoolValue(const QString &key, bool value);
    int intValue(const QString &key, int defaultValue) const;
    void setIntValue(const QString &key, int value);
    QString stringValue(const QString &key, const QString &defaultValue) const;
    void setStringValue(const QString &key, const QString &value);
};
