#include "TunnelListWidget.h"

#include "ErrorNotifier.h"
#include "TunnelListModel.h"
#include "TunnelStore.h"
#include "widgets/TunnelDialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QStackedLayout>
#include <QTableView>
#include <QVBoxLayout>

TunnelListWidget::TunnelListWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_addAction = new QAction(tr("Add"), this);
    m_editAction = new QAction(tr("Edit"), this);
    m_deleteAction = new QAction(tr("Delete"), this);
    m_toggleAction = new QAction(tr("Enable"), this);

    auto *stackHost = new QWidget(this);
    auto *stack = new QStackedLayout(stackHost);
    stack->setContentsMargins(0, 0, 0, 0);

    m_emptyLabel = new QLabel(tr("Open an SSH session to manage tunnels."), stackHost);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setEnabled(false);
    m_emptyLabel->setMargin(12);

    m_listHost = new QWidget(stackHost);
    auto *listLayout = new QVBoxLayout(m_listHost);
    listLayout->setContentsMargins(0, 0, 0, 0);

    m_model = new TunnelListModel(this);
    m_table = new QTableView(m_listHost);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(TunnelListModel::NameColumn,
                                                      QHeaderView::Stretch);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    listLayout->addWidget(m_table);

    stack->addWidget(m_emptyLabel);
    stack->addWidget(m_listHost);
    layout->addWidget(stackHost, 1);

    connect(m_addAction, &QAction::triggered, this, &TunnelListWidget::addTunnel);
    connect(m_editAction, &QAction::triggered, this, &TunnelListWidget::editSelected);
    connect(m_deleteAction, &QAction::triggered, this, &TunnelListWidget::deleteSelected);
    connect(m_toggleAction, &QAction::triggered, this, &TunnelListWidget::toggleSelected);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &TunnelListWidget::onSelectionChanged);
    connect(m_table, &QTableView::customContextMenuRequested,
            this, &TunnelListWidget::onCustomContextMenu);
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &) {
        editSelected();
    });

    showEmptyState(tr("Open an SSH session to manage tunnels."));
    updateActionsEnabled();
}

void TunnelListWidget::bindSession(TerminalSessionWidget *session)
{
    if (m_session == session) {
        updateActionsEnabled();
        return;
    }

    unbindSession();
    m_session = session;
    if (!m_session) {
        return;
    }

    connect(m_session, &TerminalSessionWidget::sessionStateChanged,
            this, &TunnelListWidget::onSessionStateChanged);
    connect(m_session, &TerminalSessionWidget::tunnelStatusChanged,
            this, &TunnelListWidget::onTunnelStatusChanged);
    connect(m_session, &TerminalSessionWidget::tunnelError,
            this, &TunnelListWidget::onTunnelError);
    connect(m_session, &QObject::destroyed, this, [this]() {
        m_session = nullptr;
        m_model->clear();
        showEmptyState(tr("Open an SSH session to manage tunnels."));
        updateActionsEnabled();
    });

    reloadFromStore();
    if (isSessionConnected()) {
        showList();
    } else {
        showEmptyState(tr("Connect the session to manage tunnels."));
    }
    updateActionsEnabled();
}

void TunnelListWidget::unbindSession()
{
    if (m_session) {
        disconnect(m_session, nullptr, this, nullptr);
        m_session = nullptr;
    }
    m_model->clearRuntimeStatuses();
    m_model->clear();
    showEmptyState(tr("Open an SSH session to manage tunnels."));
    updateActionsEnabled();
}

void TunnelListWidget::addTunnel()
{
    if (!m_session) {
        return;
    }

    TunnelDialog dialog(TunnelDialog::Mode::Create, m_session->connectionId(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const TunnelDefinition def = dialog.tunnel();
    m_model->upsert(def);
    persistAll();
    showList();
    updateActionsEnabled();
    emit statusMessage(tr("Tunnel added: %1").arg(def.name));

    if (def.enabled && isSessionConnected()) {
        m_session->startTunnel(def);
    }
}

void TunnelListWidget::editSelected()
{
    const auto current = selectedTunnel();
    if (!current || !m_session) {
        return;
    }

    const QModelIndex statusIndex =
        m_model->index(m_model->rowOf(current->id), TunnelListModel::StatusColumn);
    const QString status = statusIndex.data(Qt::DisplayRole).toString();
    if (status == QLatin1String("Listening") || status == QLatin1String("Starting")) {
        QMessageBox::information(
            this,
            tr("Edit Tunnel"),
            tr("Disable the tunnel before editing."));
        return;
    }

    TunnelDialog dialog(TunnelDialog::Mode::Edit, m_session->connectionId(), this);
    dialog.setTunnel(*current);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const TunnelDefinition def = dialog.tunnel();
    m_model->upsert(def);
    persistAll();
    emit statusMessage(tr("Tunnel updated: %1").arg(def.name));

    if (def.enabled && isSessionConnected()) {
        m_session->startTunnel(def);
    }
    updateActionsEnabled();
}

void TunnelListWidget::deleteSelected()
{
    const auto current = selectedTunnel();
    if (!current) {
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        tr("Delete Tunnel"),
        tr("Delete tunnel \"%1\"?").arg(current->name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    if (m_session && isSessionConnected()) {
        m_session->stopTunnel(current->id);
    }

    m_model->removeById(current->id);
    persistAll();

    if (m_model->rowCount() == 0 && m_session) {
        showEmptyState(tr("No tunnels for this connection."));
        // Keep list host ready — user can still Add; show empty overlay text in list area.
        // Prefer showing table empty rather than blocking Add.
        showList();
    }

    emit statusMessage(tr("Tunnel deleted: %1").arg(current->name));
    updateActionsEnabled();
}

void TunnelListWidget::toggleSelected()
{
    auto current = selectedTunnel();
    if (!current || !m_session) {
        return;
    }

    TunnelDefinition def = *current;
    const QModelIndex statusIndex =
        m_model->index(m_model->rowOf(def.id), TunnelListModel::StatusColumn);
    const QString status = statusIndex.data(Qt::DisplayRole).toString();
    const bool running =
        status == QLatin1String("Listening") || status == QLatin1String("Starting");

    if (running) {
        def.enabled = false;
        m_model->upsert(def);
        persistAll();
        if (isSessionConnected()) {
            m_session->stopTunnel(def.id);
        } else {
            m_model->setRuntimeStatus(def.id, QStringLiteral("Off"), QString());
        }
        emit statusMessage(tr("Tunnel disabled: %1").arg(def.name));
    } else {
        def.enabled = true;
        m_model->upsert(def);
        persistAll();
        if (isSessionConnected()) {
            m_session->startTunnel(def);
        } else {
            m_model->setRuntimeStatus(def.id, QStringLiteral("Off"), QString());
            emit statusMessage(tr("Tunnel will enable on next connect: %1").arg(def.name));
        }
    }
    updateActionsEnabled();
}

void TunnelListWidget::onSelectionChanged()
{
    updateActionsEnabled();
}

void TunnelListWidget::onCustomContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_table->indexAt(pos);
    if (index.isValid()) {
        m_table->selectRow(index.row());
    }

    QMenu menu(this);
    menu.addAction(m_addAction);
    menu.addAction(m_editAction);
    menu.addAction(m_toggleAction);
    menu.addSeparator();
    menu.addAction(m_deleteAction);
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void TunnelListWidget::onTunnelStatusChanged(const QUuid &tunnelId, const QString &status,
                                             const QString &detail)
{
    m_model->setRuntimeStatus(tunnelId, status, detail);
    updateActionsEnabled();
    if (status == QLatin1String("Error") && !detail.isEmpty()) {
        ErrorNotifier::notify(this, tr("Tunnel"), detail, ErrorNotifier::Level::Warning);
    }
}

void TunnelListWidget::onTunnelError(const QUuid &tunnelId, const QString &message)
{
    Q_UNUSED(tunnelId);
    ErrorNotifier::notify(this, tr("Tunnel"), message, ErrorNotifier::Level::Warning);
}

void TunnelListWidget::onSessionStateChanged(TerminalSessionWidget::State state)
{
    if (state == TerminalSessionWidget::State::Connected) {
        // Keep current rows; only reset runtime when leaving Connected.
        if (m_model->rowCount() == 0) {
            reloadFromStore();
        }
        showList();
    } else {
        m_model->clearRuntimeStatuses();
        if (m_session) {
            if (m_model->rowCount() == 0) {
                reloadFromStore();
            }
            showList();
        } else {
            showEmptyState(tr("Open an SSH session to manage tunnels."));
        }
    }
    updateActionsEnabled();
}

void TunnelListWidget::reloadFromStore()
{
    if (!m_session) {
        m_model->clear();
        return;
    }
    m_model->setTunnels(TunnelStore::loadForConnection(m_session->connectionId()));
}

void TunnelListWidget::persistAll()
{
    if (!m_session) {
        return;
    }

    const QUuid connectionId = m_session->connectionId();
    QList<TunnelDefinition> all = TunnelStore::load();
    QList<TunnelDefinition> kept;
    for (const TunnelDefinition &tunnel : all) {
        if (tunnel.connectionId != connectionId) {
            kept.append(tunnel);
        }
    }
    kept.append(m_model->tunnels());
    TunnelStore::save(kept);
}

void TunnelListWidget::updateActionsEnabled()
{
    const bool hasSession = m_session != nullptr;
    const bool connected = isSessionConnected();
    const auto current = selectedTunnel();
    const bool hasSelection = current.has_value();

    m_addAction->setEnabled(hasSession);
    m_editAction->setEnabled(hasSelection);
    m_deleteAction->setEnabled(hasSelection);
    m_toggleAction->setEnabled(hasSelection && connected);

    if (hasSelection) {
        const QModelIndex statusIndex =
            m_model->index(m_model->rowOf(current->id), TunnelListModel::StatusColumn);
        const QString status = statusIndex.data(Qt::DisplayRole).toString();
        const bool running =
            status == QLatin1String("Listening") || status == QLatin1String("Starting");
        m_toggleAction->setText(running ? tr("Disable") : tr("Enable"));
    } else {
        m_toggleAction->setText(tr("Enable"));
    }
}

void TunnelListWidget::showEmptyState(const QString &message)
{
    m_emptyLabel->setText(message);
    if (auto *stack = qobject_cast<QStackedLayout *>(m_emptyLabel->parentWidget()->layout())) {
        stack->setCurrentWidget(m_emptyLabel);
    }
}

void TunnelListWidget::showList()
{
    if (auto *stack = qobject_cast<QStackedLayout *>(m_listHost->parentWidget()->layout())) {
        stack->setCurrentWidget(m_listHost);
    }
}

std::optional<TunnelDefinition> TunnelListWidget::selectedTunnel() const
{
    if (!m_table || !m_table->selectionModel()) {
        return std::nullopt;
    }
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return std::nullopt;
    }
    return m_model->tunnelAt(rows.first().row());
}

bool TunnelListWidget::isSessionConnected() const
{
    return m_session
        && m_session->sessionState() == TerminalSessionWidget::State::Connected;
}
