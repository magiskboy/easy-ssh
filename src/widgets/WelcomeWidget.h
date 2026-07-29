#pragma once

#include "ErrorNotifier.h"

#include <QUuid>
#include <QWidget>

class ConnectionModel;
class QLabel;
class QListWidget;
class QPushButton;

class WelcomeWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeWidget(QWidget *parent = nullptr);

    void setConnectionModel(ConnectionModel *model);
    void refresh();

signals:
    void openConnectionRequested(const QUuid &id);
    void createConnectionRequested();
    void showConnectionsRequested();
    void statusMessage(const QString &message, ErrorNotifier::Level level);

private slots:
    void onRecentActivated();
    void onOpenRecentClicked();

private:
    void rebuildRecentList();

    ConnectionModel *m_model = nullptr;
    QLabel *m_recentHeading = nullptr;
    QListWidget *m_recentList = nullptr;
    QPushButton *m_openRecentButton = nullptr;
    QLabel *m_emptyRecentLabel = nullptr;
};
