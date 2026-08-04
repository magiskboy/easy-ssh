// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ServiceDetailFactory.h"

#include "core/explorer/service/ServiceInspectInfo.h"
#include "core/explorer/service/ServiceParser.h"
#include "core/session/Session.h"
#include "gui/dialogs/ModelessDialog.h"
#include "gui/explorer/service/ServiceLogsDialog.h"
#include "gui/explorer/service/ServiceTableModel.h"
#include "gui/widgets/UiMetrics.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStringView>
#include <QUuid>
#include <QVBoxLayout>

namespace
{
class ElidedLabel final : public QLabel
{
public:
    explicit ElidedLabel(const QString &text, QWidget *parent = nullptr) : QLabel(parent)
    {
        setFullText(text);
        setTextInteractionFlags(Qt::TextSelectableByMouse);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

    void setFullText(const QString &text)
    {
        m_fullText = text;
        setToolTip(m_fullText == QLatin1String("—") ? QString() : m_fullText);
        updateElided();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        updateElided();
    }

private:
    void updateElided()
    {
        const int available = qMax(40, width() - 4);
        const QString elided = fontMetrics().elidedText(m_fullText, Qt::ElideMiddle, available);
        if (text() != elided) {
            QLabel::setText(elided);
        }
    }

    QString m_fullText;
};

QFrame *makeGroupFrame(const QString &title, QWidget *parent, QFormLayout *&formOut)
{
    auto *group = new QFrame(parent);
    group->setObjectName(QStringLiteral("serviceDetailGroup"));
    group->setFrameShape(QFrame::NoFrame);
    group->setStyleSheet(QStringLiteral("QFrame#serviceDetailGroup {"
                                        "  background-color: palette(alternate-base);"
                                        "  border: 1px solid palette(mid);"
                                        "  border-radius: 8px;"
                                        "}"));

    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing);
    layout->setSpacing(UiMetrics::tightSpacing);

    auto *titleLabel = new QLabel(title, group);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto *form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(UiMetrics::sectionSpacing);
    form->setVerticalSpacing(UiMetrics::tightSpacing);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    layout->addLayout(form);
    formOut = form;
    return group;
}

void addRow(QFormLayout *form, QStringView key, const QString &value)
{
    if (!form) {
        return;
    }
    auto *keyLabel = new QLabel(QString{key});
    keyLabel->setMinimumWidth(110);
    keyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *valueLabel = new ElidedLabel(ServiceParser::formatOrDash(value));
    form->addRow(keyLabel, valueLabel);
}

struct CommandStreams
{
    QByteArray stdoutBytes;
    QByteArray stderrBytes;
};

class ServiceDetailDialog final : public QDialog
{
public:
    ServiceDetailDialog(Session *session, const ServiceInfo &seed, QWidget *parent)
        : QDialog(parent), m_session(session), m_seed(seed)
    {
        configureModelessDialog(this);
        setWindowTitle(seed.unit);
        setMinimumHeight(360);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(UiMetrics::sectionSpacing,
                                 UiMetrics::sectionSpacing,
                                 UiMetrics::sectionSpacing,
                                 UiMetrics::sectionSpacing);
        root->setSpacing(UiMetrics::sectionSpacing);

        m_statusLabel = new QLabel(tr("Loading service details…"), this);
        root->addWidget(m_statusLabel);

        auto *scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        m_content = new QWidget(scroll);
        m_contentLayout = new QVBoxLayout(m_content);
        m_contentLayout->setContentsMargins(0, 0, 0, 0);
        m_contentLayout->setSpacing(UiMetrics::sectionSpacing);
        scroll->setWidget(m_content);
        root->addWidget(scroll, 1);

        ServiceInspectInfo seedInspect;
        seedInspect.base = seed;
        rebuildContent(seedInspect);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
        auto *viewLogsButton = buttons->addButton(tr("View logs"), QDialogButtonBox::ActionRole);
        const QString logsCommand = ServiceParser::followLogsCommand(seed);
        const bool canViewLogs =
            m_session && m_session->state() == SessionState::Connected && !logsCommand.isEmpty();
        viewLogsButton->setEnabled(canViewLogs);
        if (!canViewLogs) {
            viewLogsButton->setToolTip(tr("Live logs require a connected session and systemd."));
        }
        connect(viewLogsButton, &QPushButton::clicked, this, [this]() {
            if (!m_session) {
                return;
            }
            auto *logs = new ServiceLogsDialog(m_session, m_seed, window());
            logs->show();
            logs->raise();
            logs->activateWindow();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        root->addWidget(buttons);

        if (!m_session) {
            m_statusLabel->setText(tr("No session available for inspect."));
            return;
        }

        const QString command = ServiceParser::inspectCommand(seed);
        if (command.isEmpty()) {
            m_statusLabel->setText(tr("Inspect is not supported for this service manager."));
            return;
        }

        m_requestId = QStringLiteral("service-inspect-%1")
                          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        connect(m_session,
                &Session::commandFinished,
                this,
                [this](const QString &requestId,
                       int exitStatus,
                       const QByteArray &stdoutBytes,
                       const QByteArray &stderrBytes,
                       const QString &errorMessage) {
                    onCommandFinished(requestId,
                                      exitStatus,
                                      CommandStreams{stdoutBytes, stderrBytes},
                                      errorMessage);
                });
        m_session->execCommand(m_requestId, command);
    }

private:
    void onCommandFinished(const QString &requestId,
                           int exitStatus,
                           const CommandStreams &streams,
                           const QString &errorMessage)
    {
        if (requestId != m_requestId) {
            return;
        }
        m_requestId.clear();

        if ((!errorMessage.isEmpty() && exitStatus < 0) || exitStatus != 0) {
            QString message;
            ServiceParser::classifyFailure(exitStatus, streams.stderrBytes, errorMessage, &message);
            m_statusLabel->setText(message.isEmpty() ? tr("Inspect failed.") : message);
            return;
        }

        ServiceInspectInfo inspect;
        QString parseError;
        if (!ServiceParser::parseInspect(streams.stdoutBytes, m_seed, &inspect, &parseError)) {
            m_statusLabel->setText(parseError.isEmpty() ? tr("Failed to parse inspect output.")
                                                        : parseError);
            return;
        }

        m_statusLabel->hide();
        rebuildContent(inspect);
    }

    void clearContent()
    {
        while (QLayoutItem *item = m_contentLayout->takeAt(0)) {
            if (QWidget *widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }
    }

    void rebuildContent(const ServiceInspectInfo &info)
    {
        clearContent();

        QFormLayout *identityForm = nullptr;
        m_contentLayout->addWidget(makeGroupFrame(tr("Identity"), m_content, identityForm));
        addRow(identityForm, tr("Manager"), info.base.manager);
        addRow(identityForm, tr("Unit"), info.base.unit);
        addRow(identityForm, tr("Description"), info.base.description);
        addRow(identityForm, tr("Fragment"), info.fragmentPath);

        QFormLayout *statusForm = nullptr;
        m_contentLayout->addWidget(makeGroupFrame(tr("Status"), m_content, statusForm));
        addRow(statusForm,
               tr("Active"),
               ServiceParser::formatActiveStateDisplay(info.base.activeState));
        addRow(statusForm, tr("Sub"), info.base.subState);
        addRow(statusForm, tr("Load"), info.base.loadState);
        addRow(statusForm, tr("Enabled"), info.base.unitFileState);
        addRow(statusForm,
               tr("Main PID"),
               info.base.mainPid > 0 ? QString::number(info.base.mainPid) : QString());
        addRow(statusForm, tr("Active since"), info.activeEnterTimestamp);
        addRow(statusForm, tr("Main started"), info.execMainStartTimestamp);

        QFormLayout *configForm = nullptr;
        m_contentLayout->addWidget(makeGroupFrame(tr("Config"), m_content, configForm));
        addRow(configForm, tr("Type"), info.type);
        addRow(configForm, tr("Restart"), info.restart);
        addRow(configForm, tr("Remain after exit"), info.remainAfterExit ? tr("Yes") : tr("No"));

        m_contentLayout->addStretch(1);
    }

    QPointer<Session> m_session;
    ServiceInfo m_seed;
    QString m_requestId;
    QLabel *m_statusLabel = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
};
} // namespace

ServiceDetailFactory::ServiceDetailFactory(Session *session) : m_session(session) {}

QDialog *ServiceDetailFactory::createDetailDialog(QAbstractItemModel *source,
                                                  const QModelIndex &sourceIndex,
                                                  QWidget *parent)
{
    auto *model = qobject_cast<ServiceTableModel *>(source);
    if (!model || !sourceIndex.isValid()) {
        return nullptr;
    }

    const auto service = model->serviceAt(sourceIndex.row());
    if (!service) {
        return nullptr;
    }

    return new ServiceDetailDialog(m_session, *service, parent);
}
