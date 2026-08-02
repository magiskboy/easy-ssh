/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QDialog>
#include <QHash>
#include <QString>

class CategoryDialogShell;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QKeySequenceEdit;
class QLineEdit;
class QSpinBox;
class QTreeWidget;

class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr, const QString &initialCategoryId = {});

    void selectCategory(const QString &categoryId);

private slots:
    void apply();
    void accept() override;
    void browseDownloadDir();
    void clearDownloadDir();
    void resetShortcutsDefaults();

private:
    QWidget *createFileExplorerPage();
    QWidget *createShellAppearancePage();
    QWidget *createShellBehaviorPage();
    QWidget *createGeneralPage();
    QWidget *createShortcutsPage();
    void loadFromSettings();
    void saveToSettings();
    void loadShortcutsFromSettings();
    void saveShortcutsToSettings();

    CategoryDialogShell *m_shell = nullptr;

    // File Explorer
    QCheckBox *m_showSize = nullptr;
    QCheckBox *m_showPermissions = nullptr;
    QCheckBox *m_showModified = nullptr;
    QCheckBox *m_showHidden = nullptr;
    QLineEdit *m_downloadDir = nullptr;

    // Terminal / Shell
    QFontComboBox *m_fontCombo = nullptr;
    QSpinBox *m_fontSize = nullptr;
    QComboBox *m_colorScheme = nullptr;
    QSpinBox *m_historySize = nullptr;
    QComboBox *m_cursorShape = nullptr;
    QCheckBox *m_cursorBlink = nullptr;
    QCheckBox *m_confirmMultilinePaste = nullptr;
    QCheckBox *m_smartLayout = nullptr;

    // General
    QCheckBox *m_autoReconnect = nullptr;

    // Shortcuts
    QTreeWidget *m_shortcutsTree = nullptr;
    QHash<QString, QKeySequenceEdit *> m_shortcutEditors;
};
