// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ContainerDetailFactory.h"

#include "core/explorer/container/ContainerInspectInfo.h"
#include "core/explorer/container/ContainerParser.h"
#include "core/session/Session.h"
#include "gui/dialogs/ModelessDialog.h"
#include "gui/explorer/container/ContainerTableModel.h"
#include "gui/widgets/UiMetrics.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
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
    group->setObjectName(QStringLiteral("containerDetailGroup"));
    group->setFrameShape(QFrame::NoFrame);
    group->setStyleSheet(QStringLiteral("QFrame#containerDetailGroup {"
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

    auto *valueLabel = new ElidedLabel(ContainerParser::formatOrDash(value));
    form->addRow(keyLabel, valueLabel);
}

struct CommandStreams
{
    QByteArray stdoutBytes;
    QByteArray stderrBytes;
};

class ContainerDetailDialog final : public QDialog
{
public:
    ContainerDetailDialog(Session *session, const ContainerInfo &seed, QWidget *parent)
        : QDialog(parent), m_session(session), m_seed(seed)
    {
        configureModelessDialog(this);
        setWindowTitle(ContainerParser::displayName(seed));
        setMinimumHeight(420);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(UiMetrics::sectionSpacing,
                                 UiMetrics::sectionSpacing,
                                 UiMetrics::sectionSpacing,
                                 UiMetrics::sectionSpacing);
        root->setSpacing(UiMetrics::sectionSpacing);

        m_statusLabel = new QLabel(tr("Loading inspect details…"), this);
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

        ContainerInspectInfo seedInspect;
        seedInspect.base = seed;
        seedInspect.imageName = seed.image;
        rebuildContent(seedInspect);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        root->addWidget(buttons);

        if (!m_session) {
            m_statusLabel->setText(tr("No session available for inspect."));
            return;
        }

        const QString command = ContainerParser::inspectCommand(seed);
        if (command.isEmpty()) {
            m_statusLabel->setText(tr("Inspect is not supported for this runtime."));
            return;
        }

        m_requestId = QStringLiteral("container-inspect-%1")
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
            ContainerParser::classifyFailure(
                exitStatus, streams.stderrBytes, errorMessage, &message);
            m_statusLabel->setText(message.isEmpty() ? tr("Inspect failed.") : message);
            return;
        }

        ContainerInspectInfo inspect;
        QString parseError;
        if (!ContainerParser::parseInspect(streams.stdoutBytes, m_seed, &inspect, &parseError)) {
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

    void rebuildContent(const ContainerInspectInfo &info)
    {
        clearContent();

        QFormLayout *identityForm = nullptr;
        m_contentLayout->addWidget(makeGroupFrame(tr("Identity"), m_content, identityForm));
        addRow(identityForm, tr("Runtime"), info.base.runtime);
        addRow(identityForm, tr("ID"), info.base.containerId);
        addRow(identityForm, tr("Name"), ContainerParser::displayName(info.base));
        if (!info.base.runtimeNamespace.isEmpty()) {
            addRow(identityForm, tr("Namespace"), info.base.runtimeNamespace);
        }
        addRow(identityForm, tr("Created"), info.createdAt);
        addRow(identityForm, tr("Labels"), info.labels);

        QFormLayout *imageForm = nullptr;
        m_contentLayout->addWidget(makeGroupFrame(tr("Image"), m_content, imageForm));
        addRow(imageForm, tr("Image"), info.imageName.isEmpty() ? info.base.image : info.imageName);
        addRow(imageForm, tr("Image ID"), info.imageId);
        addRow(imageForm, tr("Driver"), info.driver);
        addRow(imageForm, tr("OCI Runtime"), info.ociRuntime);

        QFormLayout *statusForm = nullptr;
        m_contentLayout->addWidget(makeGroupFrame(tr("Status"), m_content, statusForm));
        addRow(statusForm, tr("State"), ContainerParser::formatStateDisplay(info.base.state));
        addRow(statusForm, tr("CPU %"), ContainerParser::formatCpuDisplay(info.base.cpuPercent));
        addRow(statusForm,
               tr("Memory %"),
               ContainerParser::formatMemPercentDisplay(info.base.memPercent));
        addRow(statusForm, tr("Memory usage"), info.base.memUsage);
        addRow(
            statusForm, tr("PID"), info.base.pid > 0 ? QString::number(info.base.pid) : QString());
        addRow(statusForm, tr("Exit code"), QString::number(info.exitCode));
        addRow(statusForm, tr("Started"), info.startedAt);
        addRow(statusForm, tr("Finished"), info.finishedAt);
        addRow(statusForm, tr("Restarts"), QString::number(info.restartCount));
        addRow(statusForm, tr("OOM killed"), info.oomKilled ? tr("Yes") : tr("No"));
        addRow(statusForm, tr("Error"), info.stateError);

        QFormLayout *configForm = nullptr;
        m_contentLayout->addWidget(makeGroupFrame(tr("Config"), m_content, configForm));
        addRow(configForm, tr("Hostname"), info.hostname);
        addRow(configForm, tr("User"), info.user);
        addRow(configForm, tr("Working dir"), info.workingDir);
        addRow(configForm, tr("Entrypoint"), info.entrypoint);
        addRow(configForm, tr("Command"), info.command);
        addRow(configForm, tr("Env"), ContainerParser::joinElidable(info.env));

        QFormLayout *networkForm = nullptr;
        m_contentLayout->addWidget(makeGroupFrame(tr("Network"), m_content, networkForm));
        addRow(networkForm, tr("IP address"), info.ipAddress);
        addRow(networkForm, tr("Gateway"), info.gateway);
        addRow(networkForm, tr("MAC address"), info.macAddress);
        addRow(networkForm, tr("Ports"), info.ports);

        QFormLayout *mountsForm = nullptr;
        m_contentLayout->addWidget(makeGroupFrame(tr("Mounts"), m_content, mountsForm));
        if (info.mounts.isEmpty()) {
            addRow(mountsForm, tr("Mounts"), QString());
        } else {
            for (const auto &mount : info.mounts) {
                addRow(mountsForm, mount.first, mount.second);
            }
        }

        m_contentLayout->addStretch(1);
    }

    QPointer<Session> m_session;
    ContainerInfo m_seed;
    QString m_requestId;
    QLabel *m_statusLabel = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
};
} // namespace

ContainerDetailFactory::ContainerDetailFactory(Session *session) : m_session(session) {}

QDialog *ContainerDetailFactory::createDetailDialog(QAbstractItemModel *source,
                                                    const QModelIndex &sourceIndex,
                                                    QWidget *parent)
{
    auto *model = qobject_cast<ContainerTableModel *>(source);
    if (!model || !sourceIndex.isValid()) {
        return nullptr;
    }

    const auto container = model->containerAt(sourceIndex.row());
    if (!container) {
        return nullptr;
    }

    return new ContainerDetailDialog(m_session, *container, parent);
}
