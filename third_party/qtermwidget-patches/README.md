# QTermWidget patches for Easy SSH

Applied on top of [lxqt/qtermwidget](https://github.com/lxqt/qtermwidget) tag `2.4.0`
by `.github/scripts/build-qtermwidget.sh`.

| Patch | Purpose |
|-------|---------|
| `0001-…` | Public `writeToEmulator` / `setEmulationSize` for PTY-less SSH teletype |
| `0002-…` | Windows build: stub KPty/Pty (no ConPTY), History/wcwidth fixes |
| `0003-…` | Remove leftover unconditional `sys/mman.h` include left by 0002 on 2.4.0 |
| `0004-…` | Windows: guard `unistd.h` in Vt102Emulation and SIGHUP/SIGKILL in Session::close |

On Linux/macOS only the API from 0001 is required at runtime; 0002–0004 are inert
(Unix sources remain selected by CMake).
