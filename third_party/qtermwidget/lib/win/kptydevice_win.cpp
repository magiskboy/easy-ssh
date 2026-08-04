/*
 * Windows stub for KPtyDevice — no local PTY I/O.
 */

#include "kpty_p.h"
#include "kptydevice.h"

#include <QSocketNotifier>

KPtyDevice::KPtyDevice(QObject *parent) : QIODevice(parent), KPty(new KPtyDevicePrivate(this)) {}

KPtyDevice::~KPtyDevice()
{
    close();
}

bool KPtyDevice::open(OpenMode mode)
{
    Q_D(KPtyDevice);
    if (!KPty::open()) {
        return false;
    }
    d->finishOpen(mode);
    return QIODevice::open(mode | Unbuffered);
}

bool KPtyDevice::open(int, OpenMode mode)
{
    return open(mode);
}

void KPtyDevicePrivate::finishOpen(QIODevice::OpenMode)
{
    readNotifier = nullptr;
    writeNotifier = nullptr;
}

void KPtyDevice::close()
{
    Q_D(KPtyDevice);
    delete d->readNotifier;
    delete d->writeNotifier;
    d->readNotifier = nullptr;
    d->writeNotifier = nullptr;
    d->readBuffer.clear();
    d->writeBuffer.clear();
    KPty::close();
    QIODevice::close();
}

void KPtyDevice::setSuspended(bool) {}

bool KPtyDevice::isSuspended() const
{
    return true;
}

bool KPtyDevice::isSequential() const
{
    return true;
}

bool KPtyDevice::canReadLine() const
{
    Q_D(const KPtyDevice);
    return d->readBuffer.canReadLine();
}

bool KPtyDevice::atEnd() const
{
    Q_D(const KPtyDevice);
    return QIODevice::atEnd() && d->readBuffer.isEmpty();
}

qint64 KPtyDevice::bytesAvailable() const
{
    Q_D(const KPtyDevice);
    return QIODevice::bytesAvailable() + d->readBuffer.size();
}

qint64 KPtyDevice::bytesToWrite() const
{
    Q_D(const KPtyDevice);
    return d->writeBuffer.size();
}

bool KPtyDevice::waitForBytesWritten(int)
{
    return false;
}

bool KPtyDevice::waitForReadyRead(int)
{
    return false;
}

qint64 KPtyDevice::readData(char *, qint64)
{
    return 0;
}

qint64 KPtyDevice::readLineData(char *, qint64)
{
    return -1;
}

qint64 KPtyDevice::writeData(const char *, qint64)
{
    return -1;
}

bool KPtyDevicePrivate::_k_canRead()
{
    return false;
}

bool KPtyDevicePrivate::_k_canWrite()
{
    return false;
}

bool KPtyDevicePrivate::doWait(int, bool)
{
    return false;
}
