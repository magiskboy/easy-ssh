# QTermWidget patches for Easy SSH

Applied on top of [lxqt/qtermwidget](https://github.com/lxqt/qtermwidget) tag `2.4.0`
by [`cmake/patch-qtermwidget.cmake`](../cmake/patch-qtermwidget.cmake) during
FetchContent configure (when `EASY_SSH_USE_SYSTEM_PACKAGES=OFF`).

| Patch | Purpose |
|-------|---------|
| `0001-…` | Public `writeToEmulator` / `setEmulationSize` for PTY-less SSH teletype |
| `0002-…` | Windows build: stub KPty/Pty (no ConPTY), History/wcwidth fixes |
| `0003-…` | Remove leftover unconditional `sys/mman.h` include left by 0002 on 2.4.0 |
| `0004-…` | Windows: guard `unistd.h` in Vt102Emulation and SIGHUP/SIGKILL in Session::close |
| `0005-…` | Relative `install()` DESTINATION for data/translations so CPack NSIS works |

On Linux/macOS only the API from 0001 is required at runtime; 0002–0004 are inert
(Unix sources remain selected by CMake). 0005 is needed on Windows packaging
(CPack NSIS forbids absolute install destinations).

When building with distro packages (`EASY_SSH_USE_SYSTEM_PACKAGES=ON`), ensure your
packaged QTermWidget includes patch 0001 or equivalent upstream support.
