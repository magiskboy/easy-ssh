// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        NavigationSplitView {
            List(selection: $appModel.sidebarMode) {
                ForEach(SidebarMode.allCases) { mode in
                    Label(mode.title, systemImage: mode.systemImage)
                        .tag(mode)
                }
            }
            .navigationSplitViewColumnWidth(min: 140, ideal: 160, max: 220)
            .safeAreaInset(edge: .bottom) {
                Button {
                    appModel.openConnectSheet()
                } label: {
                    Label("New Connection", systemImage: "plus.circle.fill")
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
                .buttonStyle(.borderless)
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
        .sheet(isPresented: $appModel.showConnectSheet) {
            ConnectSheet()
                .environmentObject(appModel)
        }
    }
}
