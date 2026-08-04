/* Minimal termios stand-in for Windows builds (no real PTY). */
#ifndef QTERMWIDGET_WIN_TERMIOS_STUB_H
#define QTERMWIDGET_WIN_TERMIOS_STUB_H

#ifdef _WIN32

#include <cstdint>

struct termios
{
    unsigned int c_iflag = 0;
    unsigned int c_oflag = 0;
    unsigned int c_cflag = 0;
    unsigned int c_lflag = 0;
    unsigned char c_cc[32]{};
};

#ifndef IXON
#define IXON 0x0400
#endif
#ifndef IXOFF
#define IXOFF 0x1000
#endif
#ifndef IUTF8
#define IUTF8 0x4000
#endif
#ifndef VERASE
#define VERASE 2
#endif

#endif /* _WIN32 */

#endif /* QTERMWIDGET_WIN_TERMIOS_STUB_H */
