/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QPointer>
#include <QWidget>

#include <memory>

class ExplorerListWidget;
class ExplorerTableModel;
class IExplorerModule;
class IExplorerSource;
class QHideEvent;
class QShowEvent;
class Session;

/// Wires an IExplorerModule into ExplorerListWidget for a Session.
class ExplorerPageWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ExplorerPageWidget(QWidget *parent = nullptr);
    ~ExplorerPageWidget() override;

    void bind(std::unique_ptr<IExplorerModule> module, Session *session);
    void unbind();

    IExplorerModule *module() const { return m_module.get(); }
    ExplorerListWidget *listWidget() const { return m_list; }
    bool isBound() const { return m_module != nullptr && m_session != nullptr; }

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onCapabilityChanged();
    void onFailed(const QString &error);

private:
    void applyCapabilityUi();
    void stopSource();

    ExplorerListWidget *m_list = nullptr;
    std::unique_ptr<IExplorerModule> m_module;
    QPointer<Session> m_session;
    ExplorerTableModel *m_model = nullptr;
    IExplorerSource *m_source = nullptr;
};
