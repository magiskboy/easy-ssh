// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "CustomPaletteDialog.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

CustomPaletteDialog::CustomPaletteDialog(const Theme &initial, QWidget *parent)
    : QDialog(parent), m_theme(initial)
{
    setWindowTitle(tr("Customize Palette"));
    resize(420, 560);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);

    auto addGroup = [this, contentLayout](const QString &title, auto builder) {
        auto *group = new QGroupBox(title, contentLayout->parentWidget());
        auto *form = new QFormLayout(group);
        builder(form);
        contentLayout->addWidget(group);
    };

    addGroup(tr("Brand"), [this](QFormLayout *form) {
        addColorRow(form, tr("Primary"), &Theme::primary);
        addColorRow(form, tr("Secondary"), &Theme::secondary);
    });

    addGroup(tr("Accents"), [this](QFormLayout *form) {
        addColorRow(form, tr("Magenta"), &Theme::magenta);
        addColorRow(form, tr("Red"), &Theme::red);
        addColorRow(form, tr("Orange"), &Theme::orange);
        addColorRow(form, tr("Yellow"), &Theme::yellow);
        addColorRow(form, tr("Green"), &Theme::green);
        addColorRow(form, tr("Cyan"), &Theme::cyan);
        addColorRow(form, tr("Blue"), &Theme::blue);
    });

    addGroup(tr("Text"), [this](QFormLayout *form) {
        addColorRow(form, tr("Text"), &Theme::text);
        addColorRow(form, tr("Subtext 1"), &Theme::subtext1);
        addColorRow(form, tr("Subtext 0"), &Theme::subtext0);
    });

    addGroup(tr("Overlays"), [this](QFormLayout *form) {
        addColorRow(form, tr("Overlay 2"), &Theme::overlay2);
        addColorRow(form, tr("Overlay 1"), &Theme::overlay1);
        addColorRow(form, tr("Overlay 0"), &Theme::overlay0);
    });

    addGroup(tr("Surfaces"), [this](QFormLayout *form) {
        addColorRow(form, tr("Surface 2"), &Theme::surface2);
        addColorRow(form, tr("Surface 1"), &Theme::surface1);
        addColorRow(form, tr("Surface 0"), &Theme::surface0);
    });

    addGroup(tr("Background"), [this](QFormLayout *form) {
        addColorRow(form, tr("Base"), &Theme::base);
        addColorRow(form, tr("Mantle"), &Theme::mantle);
        addColorRow(form, tr("Crust"), &Theme::crust);
    });

    contentLayout->addStretch(1);
    scroll->setWidget(content);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addWidget(scroll, 1);
    root->addWidget(buttons);
}

void CustomPaletteDialog::addColorRow(QFormLayout *form, const QString &label, QColor Theme::*field)
{
    auto *button = new QPushButton(form->parentWidget());
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    button->setAutoFillBackground(true);
    button->setMinimumSize(72, 22);
    button->setToolTip((m_theme.*field).name(QColor::HexRgb));
    updateSwatch(button, m_theme.*field);
    connect(
        button, &QPushButton::clicked, this, [this, field, button]() { pickColor(field, button); });
    form->addRow(label, button);
}

void CustomPaletteDialog::updateSwatch(QPushButton *button, const QColor &color)
{
    QPalette buttonPalette = button->palette();
    buttonPalette.setColor(QPalette::Button, color);
    buttonPalette.setColor(QPalette::ButtonText, color.lightness() < 128 ? Qt::white : Qt::black);
    button->setPalette(buttonPalette);
    button->setText(color.name(QColor::HexRgb));
    button->setToolTip(color.name(QColor::HexRgb));
}

void CustomPaletteDialog::pickColor(QColor Theme::*field, QPushButton *button)
{
    const QColor chosen = QColorDialog::getColor(m_theme.*field, this, tr("Choose Color"));
    if (!chosen.isValid()) {
        return;
    }
    m_theme.*field = chosen;
    updateSwatch(button, chosen);
}
