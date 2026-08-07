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
class QPushButton;
class QSpinBox;
class QTreeWidget;
class QWidget;

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
    void onUiFontModeChanged();
    void onPaletteSelectionChanged();
    void customizePalette();
    void resetShortcutsDefaults();

private:
    QWidget *createGeneralPage();
    QWidget *createFileExplorerPage();
    QWidget *createTerminalPage();
    QWidget *createShortcutsPage();
    void loadFromSettings();
    void saveToSettings();
    void loadShortcutsFromSettings();
    void saveShortcutsToSettings();
    void updateUiFontControls();
    void updatePaletteControls();

    CategoryDialogShell *m_shell = nullptr;

    // File Explorer
    QCheckBox *m_showSize = nullptr;
    QCheckBox *m_showPermissions = nullptr;
    QCheckBox *m_showModified = nullptr;
    QCheckBox *m_showHidden = nullptr;
    QLineEdit *m_downloadDir = nullptr;

    // General — Appearance
    QComboBox *m_uiFontModeCombo = nullptr;
    QWidget *m_uiFontCustomRow = nullptr;
    QFontComboBox *m_uiFontCombo = nullptr;
    QSpinBox *m_uiFontSize = nullptr;
    QComboBox *m_paletteCombo = nullptr;
    QPushButton *m_customizePaletteButton = nullptr;
    /// Theme id to seed Customize when no custom palette exists yet.
    QString m_paletteSeedThemeId;

    // General — Session / Transfers
    QCheckBox *m_autoReconnect = nullptr;
    QCheckBox *m_restoreWorkspace = nullptr;
    QCheckBox *m_closeToTray = nullptr;
    QCheckBox *m_minimizeToTray = nullptr;
    QCheckBox *m_startInTray = nullptr;
    QCheckBox *m_trayNotifications = nullptr;
    QSpinBox *m_stallTimeout = nullptr;
    QCheckBox *m_autoResumeTransfer = nullptr;

    // Terminal
    QFontComboBox *m_fontCombo = nullptr;
    QSpinBox *m_fontSize = nullptr;
    QComboBox *m_colorScheme = nullptr;
    QSpinBox *m_historySize = nullptr;
    QComboBox *m_cursorShape = nullptr;
    QCheckBox *m_cursorBlink = nullptr;
    QCheckBox *m_confirmMultilinePaste = nullptr;
    QCheckBox *m_smartLayout = nullptr;

    // Shortcuts
    QTreeWidget *m_shortcutsTree = nullptr;
    QHash<QString, QKeySequenceEdit *> m_shortcutEditors;
};
