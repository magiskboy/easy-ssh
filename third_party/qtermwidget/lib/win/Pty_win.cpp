/*
 * Windows stub for Konsole::Pty — teletype without a local PTY.
 */

#include "Pty.h"
#include "win/termios_stub.h"

#include <QStringList>

using namespace Konsole;

void Pty::setWindowSize(int lines, int cols)
{
    _windowColumns = cols;
    _windowLines = lines;
}

QSize Pty::windowSize() const
{
    return {_windowColumns, _windowLines};
}

void Pty::setFlowControlEnabled(bool enable)
{
    _xonXoff = enable;
}

bool Pty::flowControlEnabled() const
{
    return _xonXoff;
}

void Pty::setUtf8Mode(bool enable)
{
    _utf8 = enable;
}

void Pty::setErase(char erase)
{
    _eraseChar = erase;
}

char Pty::erase() const
{
    return _eraseChar;
}

void Pty::addEnvironmentVariables(const QStringList &) {}

int Pty::start(const QString &, const QStringList &, const QStringList &, ulong, bool)
{
    return -1;
}

void Pty::setEmptyPTYProperties()
{
    // No termios / local PTY on Windows — remote SSH owns the PTY.
}

void Pty::setWriteable(bool) {}

Pty::Pty(int masterFd, QObject *parent) : KPtyProcess(masterFd, parent)
{
    init();
}

Pty::Pty(QObject *parent) : KPtyProcess(parent)
{
    init();
}

void Pty::init()
{
    _windowColumns = 0;
    _windowLines = 0;
    _eraseChar = 0;
    _xonXoff = true;
    _utf8 = true;
}

Pty::~Pty() = default;

void Pty::setInitialWorkingDirectory(const QString &) {}

int Pty::foregroundProcessGroup() const
{
    return 0;
}

void Pty::closePty()
{
    if (pty()) {
        pty()->close();
    }
}

void Pty::lockPty(bool) {}

void Pty::sendData(const char *, int) {}

void Pty::dataReceived() {}
