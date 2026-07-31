/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QStackedWidget;

class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void apply();
    void accept() override;
    void browseDownloadDir();
    void clearDownloadDir();

private:
    QWidget *createFileExplorerPage();
    QWidget *createTerminalPage();
    QWidget *createGeneralPage();
    void loadFromSettings();
    void saveToSettings();

    QListWidget *m_categoryList = nullptr;
    QStackedWidget *m_pages = nullptr;

    // File Explorer
    QCheckBox *m_showSize = nullptr;
    QCheckBox *m_showPermissions = nullptr;
    QCheckBox *m_showModified = nullptr;
    QCheckBox *m_showHidden = nullptr;
    QLineEdit *m_downloadDir = nullptr;

    // Terminal
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
};
