/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "gui/widgets/IExplorerDetailFactory.h"

#include <QPointer>

class Session;

class ServiceDetailFactory final : public IExplorerDetailFactory
{
public:
    explicit ServiceDetailFactory(Session *session);

    QDialog *createDetailDialog(QAbstractItemModel *source,
                                const QModelIndex &sourceIndex,
                                QWidget *parent) override;

private:
    QPointer<Session> m_session;
};
