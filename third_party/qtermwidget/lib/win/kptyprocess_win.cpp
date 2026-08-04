/*
 * Windows stub for KPtyProcess — no Unix childProcessModifier / dup2.
 */

#include "kprocess.h"
#include "kptydevice.h"
#include "kptyprocess.h"

KPtyProcess::KPtyProcess(QObject *parent) : KPtyProcess(-1, parent) {}

KPtyProcess::KPtyProcess(int, QObject *parent) : KProcess(parent), d_ptr(new KPtyProcessPrivate)
{
    Q_D(KPtyProcess);
    d->pty = std::make_unique<KPtyDevice>(this);
    d->pty->open();
}

KPtyProcess::~KPtyProcess() = default;

void KPtyProcess::setPtyChannels(PtyChannels channels)
{
    Q_D(KPtyProcess);
    d->ptyChannels = channels;
}

KPtyProcess::PtyChannels KPtyProcess::ptyChannels() const
{
    Q_D(const KPtyProcess);
    return d->ptyChannels;
}

void KPtyProcess::setUseUtmp(bool value)
{
    Q_D(KPtyProcess);
    d->addUtmp = value;
}

bool KPtyProcess::isUseUtmp() const
{
    Q_D(const KPtyProcess);
    return d->addUtmp;
}

KPtyDevice *KPtyProcess::pty() const
{
    Q_D(const KPtyProcess);
    return d->pty.get();
}
