// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct PlaceholderFeatureView: View {
    let mode: SidebarMode

    var body: some View {
        ContentUnavailableView {
            Label(mode.title, systemImage: mode.systemImage)
        } description: {
            Text("\(mode.title) UI will plug into the same ESSSessionController bridge (SFTP / tunnels / explorers APIs are already forwarded).")
        }
    }
}
