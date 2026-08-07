# QTermWidget (vendored for Easy SSH)

Tree is [lxqt/qtermwidget](https://github.com/lxqt/qtermwidget) tag `2.4.0`
with Easy SSH modifications applied in-tree (no separate patch series).

Built via CMake `FetchContent` `SOURCE_DIR` → `third_party/qtermwidget`
(see `cmake/EasySshDependencies.cmake`). Base tag pin:
`EASY_SSH_QTERMWIDGET_GIT_TAG` in `cmake/EasySshVersions.cmake`.

## Easy SSH modifications

| Area | Purpose |
|------|---------|
| Public API | `writeToEmulator` / `setEmulationSize` for PTY-less SSH teletype |
| Windows | Stub KPty/Pty (no ConPTY), History/wcwidth and Unix-only include guards |
| Robustness | Guard `qBound` on resize when `windowLines > lineCount`; clamp screen-line selection past line end |
| Data paths | Search portable `share/easy-ssh/{color-schemes,kb-layouts}` (and macOS Resources) so themes/keytabs work from the build tree and fat packages |

Upstream license and file-level exceptions: see `README.md` / `LICENSE*`.
