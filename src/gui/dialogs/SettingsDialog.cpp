// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SettingsDialog.h"

#include "core/settings/AppSettings.h"
#include "gui/dialogs/CustomPaletteDialog.h"
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

#include <optional>

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

    auto *appearanceGroup = new QGroupBox(tr("Appearance"), page);
    auto *appearanceForm = new QFormLayout(appearanceGroup);

    m_uiFontModeCombo = new QComboBox(appearanceGroup);
    m_uiFontModeCombo->addItem(tr("System"), QString::fromLatin1(ThemeManager::kSystemFontMode));
    m_uiFontModeCombo->addItem(tr("Custom…"), QString::fromLatin1(ThemeManager::kCustomFontMode));
    connect(m_uiFontModeCombo,
            &QComboBox::currentIndexChanged,
            this,
            &SettingsDialog::onUiFontModeChanged);
    appearanceForm->addRow(tr("Font"), m_uiFontModeCombo);

    m_uiFontCustomRow = new QWidget(appearanceGroup);
    auto *uiFontRowLayout = new QHBoxLayout(m_uiFontCustomRow);
    uiFontRowLayout->setContentsMargins(0, 0, 0, 0);
    m_uiFontCombo = new QFontComboBox(m_uiFontCustomRow);
    m_uiFontSize = new QSpinBox(m_uiFontCustomRow);
    m_uiFontSize->setRange(6, 48);
    m_uiFontSize->setSuffix(tr(" pt"));
    uiFontRowLayout->addWidget(m_uiFontCombo, 1);
    uiFontRowLayout->addWidget(m_uiFontSize);
    appearanceForm->addRow(QString(), m_uiFontCustomRow);

    m_paletteCombo = new QComboBox(appearanceGroup);
    m_paletteCombo->addItem(tr("System"), QString::fromLatin1(ThemeManager::kSystemThemeId));
    const QStringList themes = ThemeManager::availableThemes();
    for (const QString &id : themes) {
        m_paletteCombo->addItem(ThemeManager::displayName(id), id);
    }
    m_paletteCombo->addItem(tr("Custom…"), QString::fromLatin1(ThemeManager::kCustomThemeId));
    if (themes.isEmpty()) {
        m_paletteCombo->setToolTip(
            tr("Bundled palettes were not found. Rebuild/install Easy SSH so "
               "share/easy-ssh/themes is available, or use Customize… to create one."));
    }
    connect(m_paletteCombo,
            &QComboBox::currentIndexChanged,
            this,
            &SettingsDialog::onPaletteSelectionChanged);

    auto *paletteRow = new QWidget(appearanceGroup);
    auto *paletteRowLayout = new QHBoxLayout(paletteRow);
    paletteRowLayout->setContentsMargins(0, 0, 0, 0);
    paletteRowLayout->addWidget(m_paletteCombo, 1);
    m_customizePaletteButton = new QPushButton(tr("Customize…"), paletteRow);
    connect(
        m_customizePaletteButton, &QPushButton::clicked, this, &SettingsDialog::customizePalette);
    paletteRowLayout->addWidget(m_customizePaletteButton);
    appearanceForm->addRow(tr("Palette"), paletteRow);

    m_styleCombo = new QComboBox(appearanceGroup);
    m_styleCombo->addItem(tr("Apple HIG"));
    m_styleCombo->addItem(tr("Fluent"));
    m_styleCombo->addItem(tr("Material 3"));
    m_styleCombo->setEnabled(false);
    m_styleCombo->setToolTip(tr("Widget styles will be available in a later release."));
    appearanceForm->addRow(tr("Style"), m_styleCombo);

    auto *hint = new QLabel(tr("Font applies to the application UI only (not the terminal). "
                               "Palette colors use QPalette roles from qt-themes tokens."),
                            appearanceGroup);
    hint->setWordWrap(true);
    appearanceForm->addRow(hint);

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

    layout->addWidget(appearanceGroup);
    layout->addWidget(sessionGroup);
    layout->addWidget(windowGroup);
    layout->addWidget(transferGroup);
    layout->addStretch(1);
    updateUiFontControls();
    updatePaletteControls();
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

    const QFont terminalFont = s.terminalFont();
    m_fontCombo->setCurrentFont(terminalFont);
    m_fontSize->setValue(terminalFont.pointSize() > 0 ? terminalFont.pointSize() : 10);

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

    const int fontModeIndex = m_uiFontModeCombo->findData(s.uiFontMode());
    m_uiFontModeCombo->setCurrentIndex(fontModeIndex >= 0 ? fontModeIndex : 0);
    const QFont uiFont = s.uiFont();
    if (!uiFont.family().isEmpty()) {
        m_uiFontCombo->setCurrentFont(uiFont);
    }
    m_uiFontSize->setValue(uiFont.pointSize() > 0 ? uiFont.pointSize() : 10);
    updateUiFontControls();

    const QString themeId = s.themeId();
    m_paletteSeedThemeId = themeId;
    int themeIndex = m_paletteCombo->findData(themeId);
    if (themeIndex < 0) {
        if (themeId == QLatin1String(ThemeManager::kCustomThemeId) ||
            !s.customThemePath().isEmpty() || ThemeManager::loadCustomPalette()) {
            themeIndex =
                m_paletteCombo->findData(QString::fromLatin1(ThemeManager::kCustomThemeId));
        } else if (!themeId.isEmpty() && themeId != QLatin1String(ThemeManager::kSystemThemeId)) {
            // Saved bundled theme missing from catalog — keep it selectable.
            m_paletteCombo->insertItem(
                m_paletteCombo->count() - 1, ThemeManager::displayName(themeId), themeId);
            themeIndex = m_paletteCombo->findData(themeId);
        } else {
            themeIndex =
                m_paletteCombo->findData(QString::fromLatin1(ThemeManager::kSystemThemeId));
        }
    }
    if (themeIndex >= 0) {
        m_paletteCombo->setCurrentIndex(themeIndex);
    }
    updatePaletteControls();

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

    QFont terminalFont = m_fontCombo->currentFont();
    terminalFont.setPointSize(m_fontSize->value());
    terminalFont.setStyleHint(QFont::TypeWriter);
    s.setTerminalFont(terminalFont);
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

    const QString fontMode = m_uiFontModeCombo->currentData().toString();
    s.setUiFontMode(fontMode);
    if (fontMode == QLatin1String(ThemeManager::kCustomFontMode)) {
        QFont uiFont = m_uiFontCombo->currentFont();
        uiFont.setPointSize(m_uiFontSize->value());
        s.setUiFont(uiFont);
    }

    const QString themeId = m_paletteCombo->currentData().toString();
    s.setThemeId(themeId);

    if (themeId == QLatin1String(ThemeManager::kCustomThemeId)) {
        if (!ThemeManager::loadCustomPalette() &&
            !ThemeManager::loadThemeFile(s.customThemePath().trimmed())) {
            QMessageBox::warning(this,
                                 tr("Custom palette"),
                                 tr("No custom palette is saved yet. Use Customize… to create one. "
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

void SettingsDialog::onUiFontModeChanged()
{
    updateUiFontControls();
}

void SettingsDialog::onPaletteSelectionChanged()
{
    const QString id = m_paletteCombo->currentData().toString();
    if (id != QLatin1String(ThemeManager::kCustomThemeId) &&
        id != QLatin1String(ThemeManager::kSystemThemeId)) {
        m_paletteSeedThemeId = id;
    }
    updatePaletteControls();
}

void SettingsDialog::customizePalette()
{
    const QString currentId = m_paletteCombo->currentData().toString();
    std::optional<Theme> seed;

    if (currentId == QLatin1String(ThemeManager::kCustomThemeId)) {
        seed = ThemeManager::resolveCustomPaletteSeed(m_paletteSeedThemeId);
    } else if (currentId != QLatin1String(ThemeManager::kSystemThemeId)) {
        seed = ThemeManager::loadTheme(currentId);
    } else if (!m_paletteSeedThemeId.isEmpty() &&
               m_paletteSeedThemeId != QLatin1String(ThemeManager::kCustomThemeId) &&
               m_paletteSeedThemeId != QLatin1String(ThemeManager::kSystemThemeId)) {
        seed = ThemeManager::loadTheme(m_paletteSeedThemeId);
    }

    if (!seed) {
        const QStringList ids = ThemeManager::availableThemes();
        if (!ids.isEmpty()) {
            seed = ThemeManager::loadTheme(ids.first());
        }
    }

    if (!seed) {
        QMessageBox::warning(this,
                             tr("Customize Palette"),
                             tr("No palette template is available to start from. "
                                "Rebuild/install Easy SSH so share/easy-ssh/themes is available."));
        return;
    }

    CustomPaletteDialog dialog(*seed, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString error;
    if (!ThemeManager::saveCustomPalette(dialog.theme(), &error)) {
        QMessageBox::warning(
            this, tr("Customize Palette"), tr("Could not save the custom palette:\n%1").arg(error));
        return;
    }

    const int customIndex =
        m_paletteCombo->findData(QString::fromLatin1(ThemeManager::kCustomThemeId));
    if (customIndex >= 0) {
        m_paletteCombo->setCurrentIndex(customIndex);
    }
    updatePaletteControls();
}

void SettingsDialog::updateUiFontControls()
{
    if (!m_uiFontModeCombo || !m_uiFontCustomRow) {
        return;
    }
    const bool custom =
        m_uiFontModeCombo->currentData().toString() == QLatin1String(ThemeManager::kCustomFontMode);
    m_uiFontCustomRow->setVisible(custom);
}

void SettingsDialog::updatePaletteControls()
{
    if (!m_customizePaletteButton) {
        return;
    }
    m_customizePaletteButton->setEnabled(true);
}
