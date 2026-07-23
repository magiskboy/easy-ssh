#include "SettingsDialog.h"

#include "AppSettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <qtermwidget.h>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    resize(640, 420);

    m_categoryList = new QListWidget(this);
    m_categoryList->setFixedWidth(140);
    m_categoryList->addItem(tr("File Explorer"));
    m_categoryList->addItem(tr("Terminal"));
    m_categoryList->addItem(tr("General"));
    m_categoryList->setCurrentRow(0);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(createFileExplorerPage());
    m_pages->addWidget(createTerminalPage());
    m_pages->addWidget(createGeneralPage());

    connect(m_categoryList, &QListWidget::currentRowChanged,
            m_pages, &QStackedWidget::setCurrentIndex);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SettingsDialog::apply);

    auto *contentLayout = new QHBoxLayout;
    contentLayout->addWidget(m_categoryList);
    contentLayout->addWidget(m_pages, 1);

    auto *root = new QVBoxLayout(this);
    root->addLayout(contentLayout, 1);
    root->addWidget(buttonBox);

    loadFromSettings();
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

QWidget *SettingsDialog::createTerminalPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QFormLayout(page);

    m_fontCombo = new QFontComboBox(page);
    m_fontCombo->setFontFilters(QFontComboBox::MonospacedFonts);

    m_fontSize = new QSpinBox(page);
    m_fontSize->setRange(8, 48);

    m_colorScheme = new QComboBox(page);
    const QStringList schemes = QTermWidget::availableColorSchemes();
    m_colorScheme->addItems(schemes);

    m_historySize = new QSpinBox(page);
    m_historySize->setRange(-1, 1000000);
    m_historySize->setSpecialValueText(tr("Unlimited"));
    m_historySize->setToolTip(tr("Use -1 for unlimited scrollback"));

    m_cursorShape = new QComboBox(page);
    m_cursorShape->addItem(tr("Block"), 0);
    m_cursorShape->addItem(tr("Underline"), 1);
    m_cursorShape->addItem(tr("I-Beam"), 2);

    m_cursorBlink = new QCheckBox(tr("Blink cursor"), page);
    m_confirmMultilinePaste = new QCheckBox(tr("Warn before multiline paste"), page);

    layout->addRow(tr("Font"), m_fontCombo);
    layout->addRow(tr("Font size"), m_fontSize);
    layout->addRow(tr("Color scheme"), m_colorScheme);
    layout->addRow(tr("Scrollback lines"), m_historySize);
    layout->addRow(tr("Cursor shape"), m_cursorShape);
    layout->addRow(QString(), m_cursorBlink);
    layout->addRow(QString(), m_confirmMultilinePaste);
    return page;
}

QWidget *SettingsDialog::createGeneralPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *sessionGroup = new QGroupBox(tr("Session"), page);
    auto *sessionLayout = new QVBoxLayout(sessionGroup);
    m_autoReconnect = new QCheckBox(tr("Auto reconnect when connection is lost"), sessionGroup);
    auto *hint = new QLabel(
        tr("Auto reconnect will be used in a future update."), sessionGroup);
    hint->setWordWrap(true);
    hint->setEnabled(false);
    sessionLayout->addWidget(m_autoReconnect);
    sessionLayout->addWidget(hint);

    layout->addWidget(sessionGroup);
    layout->addStretch(1);
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
    } else if (!s.colorScheme().isEmpty()) {
        m_colorScheme->addItem(s.colorScheme());
        m_colorScheme->setCurrentText(s.colorScheme());
    }

    m_historySize->setValue(s.historySize());
    m_cursorShape->setCurrentIndex(s.cursorShape());
    m_cursorBlink->setChecked(s.cursorBlink());
    m_confirmMultilinePaste->setChecked(s.confirmMultilinePaste());

    m_autoReconnect->setChecked(s.autoReconnect());
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
    s.setColorScheme(m_colorScheme->currentText());
    s.setHistorySize(m_historySize->value());
    s.setCursorShape(m_cursorShape->currentData().toInt());
    s.setCursorBlink(m_cursorBlink->isChecked());
    s.setConfirmMultilinePaste(m_confirmMultilinePaste->isChecked());

    s.setAutoReconnect(m_autoReconnect->isChecked());
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
