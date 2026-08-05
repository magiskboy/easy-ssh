// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SettingsDialog.h"

#include "core/settings/AppSettings.h"
#include "gui/dialogs/ModelessDialog.h"
#include "gui/theme/ThemeManager.h"
#include "gui/widgets/CategoryDialogShell.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSystemTrayIcon>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <qtermwidget.h>

SettingsDialog::SettingsDialog(QWidget *parent, const QString &initialCategoryId) : QDialog(parent)
{
    configureModelessDialog(this);
    setWindowTitle(tr("Settings"));
    resize(720, 480);

    m_shell = new CategoryDialogShell(this);
    m_shell->addPage(nullptr, tr("General"), createGeneralPage(), QStringLiteral("general"));
    m_shell->addPage(
        nullptr, tr("File Explorer"), createFileExplorerPage(), QStringLiteral("file-explorer"));
    m_shell->addPage(nullptr, tr("Shell"), createShellPage(), QStringLiteral("shell"));
    m_shell->addPage(nullptr, tr("Shortcuts"), createShortcutsPage(), QStringLiteral("shortcuts"));

    if (!initialCategoryId.isEmpty()) {
        m_shell->selectById(initialCategoryId);
    } else {
        m_shell->selectFirst();
    }

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply),
            &QPushButton::clicked,
            this,
            &SettingsDialog::apply);

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_shell, 1);
    root->addWidget(buttonBox);

    loadFromSettings();
}

void SettingsDialog::selectCategory(const QString &categoryId)
{
    if (!m_shell) {
        return;
    }
    if (categoryId.isEmpty()) {
        m_shell->selectFirst();
    } else {
        m_shell->selectById(categoryId);
    }
}

QWidget *SettingsDialog::createGeneralPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *themeGroup = new QGroupBox(tr("Theme"), page);
    auto *themeForm = new QFormLayout(themeGroup);

    m_themeCombo = new QComboBox(themeGroup);
    m_themeCombo->addItem(tr("System"), QString::fromLatin1(ThemeManager::kSystemThemeId));
    const QStringList themes = ThemeManager::availableThemes();
    for (const QString &id : themes) {
        m_themeCombo->addItem(ThemeManager::displayName(id), id);
    }
    m_themeCombo->addItem(tr("Custom…"), QString::fromLatin1(ThemeManager::kCustomThemeId));
    if (themes.isEmpty()) {
        m_themeCombo->setToolTip(
            tr("Bundled themes were not found. Rebuild/install Easy SSH so "
               "share/easy-ssh/themes is available, or choose a custom JSON file."));
    }
    connect(m_themeCombo,
            &QComboBox::currentIndexChanged,
            this,
            &SettingsDialog::onThemeSelectionChanged);
    themeForm->addRow(tr("Theme"), m_themeCombo);

    auto *customRow = new QWidget(themeGroup);
    auto *customLayout = new QHBoxLayout(customRow);
    customLayout->setContentsMargins(0, 0, 0, 0);
    m_customThemePath = new QLineEdit(customRow);
    m_customThemePath->setPlaceholderText(tr("Path to theme .json file"));
    m_browseThemeButton = new QPushButton(tr("Browse…"), customRow);
    connect(m_browseThemeButton, &QPushButton::clicked, this, &SettingsDialog::browseCustomTheme);
    customLayout->addWidget(m_customThemePath, 1);
    customLayout->addWidget(m_browseThemeButton);
    themeForm->addRow(tr("Custom file"), customRow);

    auto *hint =
        new QLabel(tr("Themes use QPalette color roles (Fusion). Custom files must match the "
                      "qt-themes JSON format."),
                   themeGroup);
    hint->setWordWrap(true);
    themeForm->addRow(hint);

    auto *sessionGroup = new QGroupBox(tr("Session"), page);
    auto *sessionLayout = new QVBoxLayout(sessionGroup);
    m_autoReconnect = new QCheckBox(tr("Auto reconnect when connection is lost"), sessionGroup);
    sessionLayout->addWidget(m_autoReconnect);
    m_restoreWorkspace = new QCheckBox(tr("Restore previous workspace on launch"), sessionGroup);
    m_restoreWorkspace->setToolTip(
        tr("Reopen the last open connections and shell dock layout when Easy SSH starts."));
    sessionLayout->addWidget(m_restoreWorkspace);

    auto *windowGroup = new QGroupBox(tr("Window"), page);
    auto *windowLayout = new QVBoxLayout(windowGroup);
    m_closeToTray = new QCheckBox(tr("Close window to system tray"), windowGroup);
    m_closeToTray->setToolTip(
        tr("When enabled, the title-bar close button hides Easy SSH to the system tray "
           "instead of quitting. Use File → Close or Exit to quit."));
    windowLayout->addWidget(m_closeToTray);

    m_minimizeToTray = new QCheckBox(tr("Minimize to system tray"), windowGroup);
    m_minimizeToTray->setToolTip(
        tr("When enabled, minimizing the window hides Easy SSH to the system tray "
           "instead of the taskbar."));
    windowLayout->addWidget(m_minimizeToTray);

    m_startInTray = new QCheckBox(tr("Start in system tray"), windowGroup);
    m_startInTray->setToolTip(
        tr("Launch Easy SSH hidden, with only the system tray icon visible."));
    windowLayout->addWidget(m_startInTray);

    m_trayNotifications = new QCheckBox(tr("Notify when window is hidden"), windowGroup);
    m_trayNotifications->setToolTip(
        tr("Show tray notifications for disconnects and tunnel errors while the "
           "main window is hidden."));
    windowLayout->addWidget(m_trayNotifications);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        const QString unavailable = tr("System tray is not available on this desktop environment.");
        for (QCheckBox *box :
             {m_closeToTray, m_minimizeToTray, m_startInTray, m_trayNotifications}) {
            box->setEnabled(false);
            box->setToolTip(unavailable);
        }
    }

    auto *transferGroup = new QGroupBox(tr("Transfers"), page);
    auto *transferForm = new QFormLayout(transferGroup);
    m_stallTimeout = new QSpinBox(transferGroup);
    m_stallTimeout->setRange(0, 3600);
    m_stallTimeout->setSuffix(tr(" sec"));
    m_stallTimeout->setSpecialValueText(tr("Disabled"));
    m_stallTimeout->setToolTip(
        tr("Abort a transfer if no bytes progress for this long. "
           "Blocking SFTP I/O may delay detection until the next progress tick."));
    transferForm->addRow(tr("Stall timeout"), m_stallTimeout);
    m_autoResumeTransfer =
        new QCheckBox(tr("Auto-resume interrupted transfer after reconnect"), transferGroup);
    transferForm->addRow(m_autoResumeTransfer);

    layout->addWidget(themeGroup);
    layout->addWidget(sessionGroup);
    layout->addWidget(windowGroup);
    layout->addWidget(transferGroup);
    layout->addStretch(1);
    updateCustomThemeControls();
    return page;
}

QWidget *SettingsDialog::createFileExplorerPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *columnsGroup = new QGroupBox(tr("Columns"), page);
    auto *columnsLayout = new QVBoxLayout(columnsGroup);
    m_showSize = new QCheckBox(tr("Show Size"), columnsGroup);
    m_showPermissions = new QCheckBox(tr("Show Permissions"), columnsGroup);
    m_showModified = new QCheckBox(tr("Show Modified"), columnsGroup);
    columnsLayout->addWidget(m_showSize);
    columnsLayout->addWidget(m_showPermissions);
    columnsLayout->addWidget(m_showModified);

    auto *displayGroup = new QGroupBox(tr("Display"), page);
    auto *displayLayout = new QVBoxLayout(displayGroup);
    m_showHidden = new QCheckBox(tr("Show hidden files"), displayGroup);
    displayLayout->addWidget(m_showHidden);

    auto *downloadGroup = new QGroupBox(tr("Downloads"), page);
    auto *downloadLayout = new QHBoxLayout(downloadGroup);
    m_downloadDir = new QLineEdit(downloadGroup);
    m_downloadDir->setPlaceholderText(tr("Ask every time"));
    auto *browseButton = new QPushButton(tr("Browse…"), downloadGroup);
    auto *clearButton = new QPushButton(tr("Clear"), downloadGroup);
    connect(browseButton, &QPushButton::clicked, this, &SettingsDialog::browseDownloadDir);
    connect(clearButton, &QPushButton::clicked, this, &SettingsDialog::clearDownloadDir);
    downloadLayout->addWidget(m_downloadDir, 1);
    downloadLayout->addWidget(browseButton);
    downloadLayout->addWidget(clearButton);

    layout->addWidget(columnsGroup);
    layout->addWidget(displayGroup);
    layout->addWidget(downloadGroup);
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createShellPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *fontGroup = new QGroupBox(tr("Font"), page);
    auto *fontForm = new QFormLayout(fontGroup);

    m_fontCombo = new QFontComboBox(fontGroup);
    m_fontCombo->setFontFilters(QFontComboBox::MonospacedFonts);

    m_fontSize = new QSpinBox(fontGroup);
    m_fontSize->setRange(8, 48);

    fontForm->addRow(tr("Font"), m_fontCombo);
    fontForm->addRow(tr("Font size"), m_fontSize);

    auto *colorGroup = new QGroupBox(tr("Colors"), page);
    auto *colorForm = new QFormLayout(colorGroup);

    m_colorScheme = new QComboBox(colorGroup);
    QStringList schemes = QTermWidget::availableColorSchemes();
    schemes.sort(Qt::CaseInsensitive);
    if (schemes.isEmpty()) {
        m_colorScheme->addItem(tr("No color schemes found"));
        m_colorScheme->setEnabled(false);
        m_colorScheme->setToolTip(
            tr("Bundled terminal color schemes were not found. "
               "Rebuild/install Easy SSH so share/easy-ssh/color-schemes is available."));
    } else {
        m_colorScheme->addItems(schemes);
    }
    colorForm->addRow(tr("Color scheme"), m_colorScheme);

    auto *cursorGroup = new QGroupBox(tr("Cursor"), page);
    auto *cursorForm = new QFormLayout(cursorGroup);

    m_cursorShape = new QComboBox(cursorGroup);
    m_cursorShape->addItem(tr("Block"), 0);
    m_cursorShape->addItem(tr("Underline"), 1);
    m_cursorShape->addItem(tr("I-Beam"), 2);
    m_cursorBlink = new QCheckBox(tr("Blink cursor"), cursorGroup);

    cursorForm->addRow(tr("Cursor shape"), m_cursorShape);
    cursorForm->addRow(QString(), m_cursorBlink);

    auto *scrollGroup = new QGroupBox(tr("Scrollback"), page);
    auto *scrollForm = new QFormLayout(scrollGroup);

    m_historySize = new QSpinBox(scrollGroup);
    m_historySize->setRange(-1, 1000000);
    m_historySize->setSpecialValueText(tr("Unlimited"));
    m_historySize->setToolTip(tr("Use -1 for unlimited scrollback"));
    scrollForm->addRow(tr("Scrollback lines"), m_historySize);

    auto *inputGroup = new QGroupBox(tr("Input"), page);
    auto *inputLayout = new QVBoxLayout(inputGroup);
    m_confirmMultilinePaste = new QCheckBox(tr("Warn before multiline paste"), inputGroup);
    inputLayout->addWidget(m_confirmMultilinePaste);

    auto *layoutGroup = new QGroupBox(tr("Layout"), page);
    auto *layoutLayout = new QVBoxLayout(layoutGroup);
    m_smartLayout = new QCheckBox(tr("Smart layout for new shells"), layoutGroup);
    m_smartLayout->setToolTip(
        tr("Automatically tile newly created shells next to the focused pane "
           "(alternating right / bottom). Drag from the sidebar to place manually."));
    layoutLayout->addWidget(m_smartLayout);

    layout->addWidget(fontGroup);
    layout->addWidget(colorGroup);
    layout->addWidget(cursorGroup);
    layout->addWidget(scrollGroup);
    layout->addWidget(inputGroup);
    layout->addWidget(layoutGroup);
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createShortcutsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    m_shortcutsTree = new QTreeWidget(page);
    m_shortcutsTree->setColumnCount(2);
    m_shortcutsTree->setHeaderLabels({tr("Action"), tr("Shortcut")});
    m_shortcutsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_shortcutsTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_shortcutsTree->setRootIsDecorated(true);
    m_shortcutsTree->setUniformRowHeights(true);

    auto *resetButton = new QPushButton(tr("Reset Defaults"), page);
    connect(resetButton, &QPushButton::clicked, this, &SettingsDialog::resetShortcutsDefaults);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(resetButton);
    buttonRow->addStretch(1);

    layout->addWidget(m_shortcutsTree, 1);
    layout->addLayout(buttonRow);
    return page;
}

void SettingsDialog::loadFromSettings()
{
    auto &s = AppSettings::instance();

    m_showSize->setChecked(s.showSizeColumn());
    m_showPermissions->setChecked(s.showPermissionsColumn());
    m_showModified->setChecked(s.showModifiedColumn());
    m_showHidden->setChecked(s.showHiddenFiles());
    m_downloadDir->setText(s.defaultDownloadDir());

    const QFont font = s.terminalFont();
    m_fontCombo->setCurrentFont(font);
    m_fontSize->setValue(font.pointSize() > 0 ? font.pointSize() : 10);

    const int schemeIndex = m_colorScheme->findText(s.colorScheme());
    if (schemeIndex >= 0) {
        m_colorScheme->setCurrentIndex(schemeIndex);
    } else if (m_colorScheme->isEnabled() && !s.colorScheme().isEmpty()) {
        // Saved name missing from catalog — keep it selectable so Apply can re-save.
        m_colorScheme->addItem(s.colorScheme());
        m_colorScheme->setCurrentText(s.colorScheme());
    }

    m_historySize->setValue(s.historySize());
    m_cursorShape->setCurrentIndex(s.cursorShape());
    m_cursorBlink->setChecked(s.cursorBlink());
    m_confirmMultilinePaste->setChecked(s.confirmMultilinePaste());
    m_smartLayout->setChecked(s.smartLayout());

    m_autoReconnect->setChecked(s.autoReconnect());
    m_restoreWorkspace->setChecked(s.restoreWorkspace());
    m_closeToTray->setChecked(s.closeToTray());
    m_minimizeToTray->setChecked(s.minimizeToTray());
    m_startInTray->setChecked(s.startInTray());
    m_trayNotifications->setChecked(s.trayNotifications());
    m_stallTimeout->setValue(s.transferStallTimeoutSec());
    m_autoResumeTransfer->setChecked(s.autoResumeTransferAfterReconnect());

    const QString themeId = s.themeId();
    int themeIndex = m_themeCombo->findData(themeId);
    if (themeIndex < 0) {
        if (themeId == QLatin1String(ThemeManager::kCustomThemeId) ||
            !s.customThemePath().isEmpty()) {
            themeIndex = m_themeCombo->findData(QString::fromLatin1(ThemeManager::kCustomThemeId));
        } else if (!themeId.isEmpty() && themeId != QLatin1String(ThemeManager::kSystemThemeId)) {
            // Saved bundled theme missing from catalog — keep it selectable.
            m_themeCombo->insertItem(
                m_themeCombo->count() - 1, ThemeManager::displayName(themeId), themeId);
            themeIndex = m_themeCombo->findData(themeId);
        } else {
            themeIndex = m_themeCombo->findData(QString::fromLatin1(ThemeManager::kSystemThemeId));
        }
    }
    if (themeIndex >= 0) {
        m_themeCombo->setCurrentIndex(themeIndex);
    }
    m_customThemePath->setText(s.customThemePath());
    updateCustomThemeControls();

    loadShortcutsFromSettings();
}

void SettingsDialog::saveToSettings()
{
    auto &s = AppSettings::instance();

    s.setShowSizeColumn(m_showSize->isChecked());
    s.setShowPermissionsColumn(m_showPermissions->isChecked());
    s.setShowModifiedColumn(m_showModified->isChecked());
    s.setShowHiddenFiles(m_showHidden->isChecked());
    s.setDefaultDownloadDir(m_downloadDir->text().trimmed());

    QFont font = m_fontCombo->currentFont();
    font.setPointSize(m_fontSize->value());
    font.setStyleHint(QFont::TypeWriter);
    s.setTerminalFont(font);
    if (m_colorScheme->isEnabled()) {
        s.setColorScheme(m_colorScheme->currentText());
    }
    s.setHistorySize(m_historySize->value());
    s.setCursorShape(m_cursorShape->currentData().toInt());
    s.setCursorBlink(m_cursorBlink->isChecked());
    s.setConfirmMultilinePaste(m_confirmMultilinePaste->isChecked());
    s.setSmartLayout(m_smartLayout->isChecked());

    s.setAutoReconnect(m_autoReconnect->isChecked());
    s.setRestoreWorkspace(m_restoreWorkspace->isChecked());
    s.setCloseToTray(m_closeToTray->isChecked());
    s.setMinimizeToTray(m_minimizeToTray->isChecked());
    s.setStartInTray(m_startInTray->isChecked());
    s.setTrayNotifications(m_trayNotifications->isChecked());
    s.setTransferStallTimeoutSec(m_stallTimeout->value());
    s.setAutoResumeTransferAfterReconnect(m_autoResumeTransfer->isChecked());

    const QString themeId = m_themeCombo->currentData().toString();
    s.setThemeId(themeId);
    s.setCustomThemePath(m_customThemePath->text().trimmed());

    if (themeId == QLatin1String(ThemeManager::kCustomThemeId)) {
        const QString path = m_customThemePath->text().trimmed();
        if (path.isEmpty() || !ThemeManager::loadThemeFile(path)) {
            QMessageBox::warning(this,
                                 tr("Custom theme"),
                                 tr("Could not load the custom theme file. "
                                    "The application will fall back to the system palette."));
        }
    }

    saveShortcutsToSettings();
}

void SettingsDialog::loadShortcutsFromSettings()
{
    m_shortcutsTree->clear();
    m_shortcutEditors.clear();

    QHash<QString, QTreeWidgetItem *> groupItems;
    auto &settings = AppSettings::instance();

    for (const QString &actionId : AppSettings::shortcutActionIds()) {
        const QString group = AppSettings::shortcutGroup(actionId);
        QTreeWidgetItem *groupItem = groupItems.value(group);
        if (!groupItem) {
            groupItem = new QTreeWidgetItem(m_shortcutsTree);
            groupItem->setText(0, group);
            groupItem->setFlags(Qt::ItemIsEnabled);
            groupItems.insert(group, groupItem);
        }

        auto *row = new QTreeWidgetItem(groupItem);
        row->setText(0, AppSettings::shortcutLabel(actionId));
        row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        auto *editor = new QKeySequenceEdit(settings.shortcut(actionId), m_shortcutsTree);
        m_shortcutsTree->setItemWidget(row, 1, editor);
        m_shortcutEditors.insert(actionId, editor);
    }

    m_shortcutsTree->expandAll();
}

void SettingsDialog::saveShortcutsToSettings()
{
    auto &settings = AppSettings::instance();
    for (auto it = m_shortcutEditors.cbegin(); it != m_shortcutEditors.cend(); ++it) {
        settings.setShortcut(it.key(), it.value()->keySequence());
    }
}

void SettingsDialog::resetShortcutsDefaults()
{
    AppSettings::instance().resetShortcutsToDefaults();
    for (auto it = m_shortcutEditors.begin(); it != m_shortcutEditors.end(); ++it) {
        it.value()->setKeySequence(AppSettings::defaultShortcut(it.key()));
    }
}

void SettingsDialog::apply()
{
    saveToSettings();
    AppSettings::instance().notifyChanged();
}

void SettingsDialog::accept()
{
    apply();
    QDialog::accept();
}

void SettingsDialog::browseDownloadDir()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Default Download Folder"), m_downloadDir->text());
    if (!dir.isEmpty()) {
        m_downloadDir->setText(dir);
    }
}

void SettingsDialog::clearDownloadDir()
{
    m_downloadDir->clear();
}

void SettingsDialog::browseCustomTheme()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Select Theme File"),
                                                      m_customThemePath->text(),
                                                      tr("Theme JSON (*.json);;All Files (*)"));
    if (!path.isEmpty()) {
        m_customThemePath->setText(path);
        const int customIndex =
            m_themeCombo->findData(QString::fromLatin1(ThemeManager::kCustomThemeId));
        if (customIndex >= 0) {
            m_themeCombo->setCurrentIndex(customIndex);
        }
        updateCustomThemeControls();
    }
}

void SettingsDialog::onThemeSelectionChanged()
{
    updateCustomThemeControls();
}

void SettingsDialog::updateCustomThemeControls()
{
    if (!m_themeCombo || !m_customThemePath || !m_browseThemeButton) {
        return;
    }
    const bool custom =
        m_themeCombo->currentData().toString() == QLatin1String(ThemeManager::kCustomThemeId);
    m_customThemePath->setEnabled(custom);
    m_browseThemeButton->setEnabled(custom);
}
