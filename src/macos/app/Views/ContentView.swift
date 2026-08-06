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
                case .files, .tunnels, .explorers:
                    PlaceholderFeatureView(mode: appModel.sidebarMode)
                }
            }
        }
        .safeAreaInset(edge: .bottom, spacing: 0) {
            AppStatusBar()
        }
        .background(WindowFrameTracker())
        .sheet(isPresented: $appModel.showConnectSheet) {
            ConnectSheet()
                .environmentObject(appModel)
        }
        .sheet(isPresented: $appModel.showConnectionManager) {
            ConnectionManagerView()
                .environmentObject(appModel)
        }
        .sheet(item: $appModel.passwordPrompt) { _ in
            PasswordPromptSheet()
                .environmentObject(appModel)
        }
        .sheet(isPresented: $appModel.showAbout) {
            AboutView()
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
    }
}
