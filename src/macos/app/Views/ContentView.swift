// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        NavigationSplitView {
            List(selection: appModel.sidebarModeBinding) {
                ForEach(SidebarMode.allCases) { mode in
                    Label(mode.title, systemImage: mode.systemImage)
                        .tag(mode)
                        .foregroundStyle(mode.isImplemented ? .primary : .secondary)
                        .opacity(mode.isImplemented ? 1 : 0.45)
                        .disabled(!mode.isImplemented)
                }
            }
            .navigationSplitViewColumnWidth(min: 140, ideal: 160, max: 220)
            .safeAreaInset(edge: .bottom) {
                VStack(alignment: .leading, spacing: 4) {
                    Button {
                        appModel.openConnectSheet()
                    } label: {
                        Label("New Connection", systemImage: "plus.circle.fill")
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                    .buttonStyle(.borderless)

                    Button {
                        appModel.openConnectionManager()
                    } label: {
                        Label("Browse Connections", systemImage: "list.bullet.rectangle")
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                    .buttonStyle(.borderless)
                }
                .padding(12)
            }
        } detail: {
            Group {
                switch appModel.sidebarMode {
                case .sessions:
                    SessionContainerView()
                case .files:
                    FileExplorerView()
                case .explorers:
                    ExplorerHubView()
                case .tunnels:
                    TunnelListView()
                }
            }
        }
        .safeAreaInset(edge: .bottom, spacing: 0) {
            AppStatusBar()
        }
        .background(WindowFrameTracker())
        .background(AppLifecycleBridge())
        .preferredColorScheme(appAppearanceColorScheme)
        .modifier(AppUIFontModifier(epoch: appModel.settingsEpoch))
        // Single sheet host — stacked .sheet modifiers silently fail on macOS.
        .sheet(item: $appModel.activeModal) { modal in
            switch modal {
            case .connect:
                ConnectSheet()
                    .environmentObject(appModel)
            case .connectionManager:
                ConnectionManagerView()
                    .environmentObject(appModel)
            case .passwordPrompt:
                PasswordPromptSheet()
                    .environmentObject(appModel)
            case .hostKeyPrompt:
                if let prompt = appModel.activeHostKeyPrompt {
                    HostKeySheet(prompt: prompt) { accept in
                        appModel.respondHostKey(accept: accept)
                    }
                } else {
                    Color.clear
                        .onAppear {
                            appModel.activeModal = nil
                        }
                }
            case .about:
                AboutView()
            }
        }
        .alert(item: Binding(
            get: { appModel.status.alert },
            set: { appModel.status.alert = $0 }
        )) { alert in
            Alert(
                title: Text(alert.title),
                message: Text(alert.message),
                dismissButton: .default(Text("OK")) {
                    appModel.status.dismissAlert()
                }
            )
        }
        .sheet(item: $appModel.paletteMode) { mode in
            CommandPaletteView(mode: mode)
                .environmentObject(appModel)
        }
    }

    private var appAppearanceColorScheme: ColorScheme? {
        _ = appModel.settingsEpoch
        let themeId = ESSAppSettings.shared().themeId
        return AppAppearance.preferredColorScheme(themeId: themeId)
    }
}

private struct AppUIFontModifier: ViewModifier {
    let epoch: Int

    private var resolvedFont: Font? {
        let _ = epoch
        let settings = ESSAppSettings.shared()
        return AppAppearance.uiFont(
            family: settings.uiFontFamily,
            pointSize: settings.uiFontPointSize,
            mode: settings.uiFontMode
        )
    }

    func body(content: Content) -> some View {
        Group {
            if let resolvedFont {
                content.font(resolvedFont)
            } else {
                content
            }
        }
    }
}
