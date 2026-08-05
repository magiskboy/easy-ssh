# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- System tray enhancements: live status tooltip and icon, Sessions/Recent tray menus, notifications when the window is hidden, minimize-to-tray and start-in-tray settings
- Workspace restore: reopen last sessions and shell dock layouts on launch (Settings → General)
- Workspace restore also reopens explorer tabs (Processes, Containers, Services, System Info)
- SFTP transfer resume via `.filepart` + SHA-256 prefix/full verification after cancel, stall, or disconnect
- Transfer stall timeout and auto-resume-after-reconnect settings (General → Transfers)
- Session auto-reconnect when connection is lost (honors existing setting)
- File Explorer Resume / Discard controls for interrupted transfers; conflict prompts for existing finals vs partials
- Optional “Save password” when creating/editing password auth connections (off by default; prompts at connect if not stored)

### Changed

- Explorer upload/download conflict dialogs support Overwrite / Skip / Cancel (and Resume / Restart for partials)
