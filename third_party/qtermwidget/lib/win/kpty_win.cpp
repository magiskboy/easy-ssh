/*
 * Windows stub for KPty — no local PTY; Easy SSH feeds Emulation via writeToEmulator.
 */

#include "kpty.h"
#include "kpty_p.h"
#include "win/termios_stub.h"

#include <cstring>

KPtyPrivate::KPtyPrivate(KPty *parent) : masterFd(-1), slaveFd(-1), ownMaster(true), q_ptr(parent)
{
}

KPtyPrivate::~KPtyPrivate() = default;

bool KPtyPrivate::chownpty(bool)
{
    return false;
}

KPty::KPty() : d_ptr(new KPtyPrivate(this)) {}

KPty::KPty(KPtyPrivate *d) : d_ptr(d)
{
    d_ptr->q_ptr = this;
}

KPty::~KPty()
{
    close();
}

bool KPty::open()
{
    Q_D(KPty);
    d->masterFd = -1;
    d->slaveFd = -1;
    d->ttyName.clear();
    return true;
}

bool KPty::open(int)
{
    return open();
}

void KPty::close()
{
    Q_D(KPty);
    d->masterFd = -1;
    d->slaveFd = -1;
    d->ttyName.clear();
}

void KPty::closeSlave() {}

bool KPty::openSlave()
{
    return false;
}

void KPty::setCTty() {}

void KPty::login(const char *, const char *) {}

void KPty::logout() {}

bool KPty::tcGetAttr(struct ::termios *ttmode) const
{
    if (!ttmode) {
        return false;
    }
    std::memset(ttmode, 0, sizeof(*ttmode));
    return false;
}

bool KPty::tcSetAttr(struct ::termios *)
{
    return false;
}

bool KPty::setWinSize(int, int)
{
    return false;
}

bool KPty::setEcho(bool)
{
    return false;
}

int KPty::masterFd() const
{
    Q_D(const KPty);
    return d->masterFd;
}

int KPty::slaveFd() const
{
    Q_D(const KPty);
    return d->slaveFd;
}

const char *KPty::ttyName() const
{
    Q_D(const KPty);
    return d->ttyName.constData();
}
