# QTermWidget patches for Easy SSH

Applied on top of [lxqt/qtermwidget](https://github.com/lxqt/qtermwidget) tag `2.4.0`
by [`cmake/patch-qtermwidget.cmake`](../cmake/patch-qtermwidget.cmake) during
FetchContent configure.

| Patch | Purpose |
|-------|---------|
| `0001-…` | Public `writeToEmulator` / `setEmulationSize` for PTY-less SSH teletype |
| `0002-…` | Windows build: stub KPty/Pty (no ConPTY), History/wcwidth fixes |
| `0003-…` | Remove leftover unconditional `sys/mman.h` include left by 0002 on 2.4.0 |
| `0004-…` | Windows: guard `unistd.h` in Vt102Emulation and SIGHUP/SIGKILL in Session::close |
| `0005-…` | Guard `qBound` when `windowLines > lineCount` / empty textBounds (Qt 6 assert on maximize/resize) |
| `0006-…` | Clamp screen-line selection when start is past line end (Qt 6 `qBound` assert on mouse select) |
| `0007-…` | Search portable `share/easy-ssh/{color-schemes,kb-layouts}` (and macOS Resources) so themes/keytabs work from the build tree and fat packages without a system qtermwidget install |

On Linux/macOS only the API from 0001 is required at runtime; 0002–0004 are inert
(Unix sources remain selected by CMake). 0005–0007 apply on all platforms.
