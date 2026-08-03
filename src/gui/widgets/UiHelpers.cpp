// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "UiHelpers.h"

#include "gui/widgets/UiMetrics.h"

#include <QAbstractButton>
#include <QDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLayout>
#include <QLineEdit>
#include <QStyle>
#include <QWidget>

namespace UiHelpers
{

void applyContentMargins(QLayout *layout, QWidget *styleWidget)
{
    if (!layout || !styleWidget) {
        return;
    }
    QStyle *style = styleWidget->style();
    layout->setContentsMargins(
        style->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, styleWidget),
        style->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, styleWidget),
        style->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, styleWidget),
        style->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, styleWidget));
}

QWidget *makeBrowseRow(QWidget *edit, QAbstractButton *browse, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(UiMetrics::tightSpacing);
    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    return row;
}

QString getText(QWidget *parent, const TextPrompt &prompt, bool *ok)
{
    QInputDialog dialog(parent);
    dialog.setWindowTitle(prompt.title);
    dialog.setLabelText(prompt.label);
    dialog.setTextValue(prompt.text);
    dialog.setInputMode(QInputDialog::TextInput);
    dialog.setTextEchoMode(QLineEdit::Normal);
    dialog.setMinimumWidth(UiMetrics::inputDialogMinWidth);

    const int result = dialog.exec();
    if (ok) {
        *ok = result == QDialog::Accepted;
    }
    return result == QDialog::Accepted ? dialog.textValue() : QString();
}

} // namespace UiHelpers
