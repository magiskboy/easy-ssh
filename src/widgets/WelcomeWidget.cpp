#include "WelcomeWidget.h"

#include <QLabel>
#include <QVBoxLayout>

WelcomeWidget::WelcomeWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    auto *title = new QLabel(tr("Easy SSH"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 8);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);

    auto *hint = new QLabel(tr("Double-click a connection to open a session"), this);
    hint->setAlignment(Qt::AlignCenter);
    hint->setEnabled(false);

    layout->addWidget(title);
    layout->addWidget(hint);
}
