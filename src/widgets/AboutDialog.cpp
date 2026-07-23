#include "AboutDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QtGlobal>

#include <libssh/libssh.h>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Easy SSH"));
    setModal(true);

    auto *title = new QLabel(tr("<h2>Easy SSH</h2>"), this);
    title->setTextFormat(Qt::RichText);

    const QString libsshVersion = QString::fromUtf8(ssh_version(0));
    auto *body = new QLabel(
        tr("A lightweight SSH / SFTP client.<br><br>"
           "Version: %1<br>"
           "Qt: %2<br>"
           "libssh: %3<br><br>"
           "<a href=\"https://github.com/magiskboy/easy-ssh\">"
           "github.com/magiskboy/easy-ssh</a>")
            .arg(QStringLiteral(APP_VERSION),
                 QString::fromLatin1(qVersion()),
                 libsshVersion),
        this);
    body->setTextFormat(Qt::RichText);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    body->setOpenExternalLinks(true);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto *root = new QVBoxLayout(this);
    root->addWidget(title);
    root->addWidget(body);
    root->addStretch(1);
    root->addWidget(buttonBox);
}
