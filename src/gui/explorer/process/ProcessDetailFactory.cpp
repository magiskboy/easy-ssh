// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ProcessDetailFactory.h"

#include "core/explorer/process/ProcessParser.h"
#include "gui/dialogs/ModelessDialog.h"
#include "gui/explorer/process/ProcessTableModel.h"
#include "gui/widgets/UiMetrics.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace
{
/// QScrollArea::sizeHint() follows the child widget, which would force the dialog
/// to grow to the full command list and defeat max-height / scrolling.
class ConstrainedScrollArea final : public QScrollArea
{
public:
    explicit ConstrainedScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    QSize sizeHint() const override { return {UiMetrics::dialogMinWidth, 240}; }

    QSize minimumSizeHint() const override { return {0, 0}; }
};

QWidget *makeKeyValueRow(const QPair<QString, QString> &entry, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(UiMetrics::relatedSpacing,
                               UiMetrics::tightSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::tightSpacing);
    layout->setSpacing(UiMetrics::relatedSpacing);

    auto *keyLabel = new QLabel(entry.first, row);
    keyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *valueLabel = new QLabel(entry.second, row);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    valueLabel->setWordWrap(true);

    layout->addWidget(keyLabel, 0, Qt::AlignLeft);
    layout->addStretch(1);
    layout->addWidget(valueLabel, 0, Qt::AlignRight);
    return row;
}

QFrame *makeGroupFrame(const QString &title, QWidget *parent)
{
    auto *group = new QFrame(parent);
    group->setObjectName(QStringLiteral("processDetailGroup"));
    group->setFrameShape(QFrame::NoFrame);
    group->setStyleSheet(QStringLiteral("QFrame#processDetailGroup {"
                                        "  background-color: palette(alternate-base);"
                                        "  border: 1px solid palette(mid);"
                                        "  border-radius: 8px;"
                                        "}"));

    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing);
    layout->setSpacing(0);

    auto *titleLabel = new QLabel(title, group);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);
    layout->addSpacing(UiMetrics::tightSpacing);

    return group;
}

QWidget *
makeGroup(const QString &title, const QList<QPair<QString, QString>> &rows, QWidget *parent)
{
    auto *group = makeGroupFrame(title, parent);
    auto *layout = qobject_cast<QVBoxLayout *>(group->layout());
    for (const auto &row : rows) {
        layout->addWidget(makeKeyValueRow(row, group));
    }
    return group;
}

QWidget *makeCommandGroup(const QString &command, QWidget *parent)
{
    const QStringList parts = command.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    auto *group = makeGroupFrame(QObject::tr("Command"), parent);
    auto *layout = qobject_cast<QVBoxLayout *>(group->layout());

    for (const QString &part : parts) {
        auto *row = new QWidget(group);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(UiMetrics::relatedSpacing,
                                      UiMetrics::tightSpacing,
                                      UiMetrics::relatedSpacing,
                                      UiMetrics::tightSpacing);
        rowLayout->setSpacing(0);

        auto *valueLabel = new QLabel(part, row);
        valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        valueLabel->setWordWrap(true);
        rowLayout->addWidget(valueLabel, 1);

        layout->addWidget(row);
    }

    return group;
}
} // namespace

QDialog *ProcessDetailFactory::createDetailDialog(QAbstractItemModel *source,
                                                  const QModelIndex &sourceIndex,
                                                  QWidget *parent)
{
    auto *model = qobject_cast<ProcessTableModel *>(source);
    if (!model || !sourceIndex.isValid()) {
        return nullptr;
    }

    const auto process = model->processAt(sourceIndex.row());
    if (!process) {
        return nullptr;
    }

    auto *dialog = new QDialog(parent);
    configureModelessDialog(dialog);
    dialog->setWindowTitle(ProcessParser::displayName(*process));

    QScreen *screen = parent ? parent->screen() : nullptr;
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const int screenHeight = screen ? screen->availableGeometry().height() : 900;
    const int maxHeight = qMax(360, qRound(screenHeight * 0.7));
    const int minHeight = qMin(360, maxHeight);
    const int initialHeight = qMin(480, maxHeight);
    dialog->setMinimumHeight(minHeight);
    dialog->setMaximumHeight(maxHeight);
    dialog->resize(UiMetrics::dialogMinWidth, initialHeight);

    auto *root = new QVBoxLayout(dialog);
    root->setContentsMargins(UiMetrics::sectionSpacing,
                             UiMetrics::sectionSpacing,
                             UiMetrics::sectionSpacing,
                             UiMetrics::sectionSpacing);
    root->setSpacing(UiMetrics::sectionSpacing);
    root->setSizeConstraint(QLayout::SetMinimumSize);

    auto *scroll = new ConstrainedScrollArea(dialog);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFocusPolicy(Qt::StrongFocus);
    auto *content = new QWidget(scroll);
    content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(UiMetrics::sectionSpacing);
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    const QList<QPair<QString, QString>> details = {
        {QObject::tr("Process ID"), QString::number(process->pid)},
        {QObject::tr("User"), ProcessParser::formatUserDisplay(*process)},
    };

    const QList<QPair<QString, QString>> status = {
        {QObject::tr("Started"), ProcessParser::formatStartedDisplay(process->elapsedSeconds)},
        {QObject::tr("Priority"), ProcessParser::formatPriorityDisplay(process->nice)},
        {QObject::tr("Status"), ProcessParser::formatStateDisplay(process->stateCode)},
    };

    const QList<QPair<QString, QString>> usage = {
        {QObject::tr("CPU"), QObject::tr("%1%").arg(process->cpuPercent, 0, 'f', 2)},
        {QObject::tr("Memory"), ProcessParser::formatMemoryFromKiB(process->rssKiB)},
        {QObject::tr("CPU Time"),
         process->cpuTime.isEmpty() ? QStringLiteral("—") : process->cpuTime},
        {QObject::tr("Virtual Memory"), ProcessParser::formatMemoryFromKiB(process->vszKiB)},
        {QObject::tr("Resident Memory"), ProcessParser::formatMemoryFromKiB(process->rssKiB)},
        {QObject::tr("Writable Memory"), QStringLiteral("—")},
        {QObject::tr("Shared Memory"), QStringLiteral("—")},
    };

    contentLayout->addWidget(makeGroup(QObject::tr("Details"), details, content));
    contentLayout->addWidget(makeGroup(QObject::tr("Status"), status, content));
    contentLayout->addWidget(makeGroup(QObject::tr("Usage"), usage, content));

    if (!process->command.isEmpty()) {
        contentLayout->addWidget(makeCommandGroup(process->command, content));
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    root->addWidget(buttons);

    return dialog;
}
