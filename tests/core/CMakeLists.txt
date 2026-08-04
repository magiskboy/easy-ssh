# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

function(add_easy_ssh_core_test target_name)
    qt_add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE easy-ssh-core Qt6::Test)
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

add_easy_ssh_core_test(tst_SystemInfoParser
    explorer/systeminfo/tst_SystemInfoParser.cpp
)

add_easy_ssh_core_test(tst_ProcessParser
    explorer/process/tst_ProcessParser.cpp
)

add_easy_ssh_core_test(tst_ServiceParser
    explorer/service/tst_ServiceParser.cpp
)

add_easy_ssh_core_test(tst_ContainerParser
    explorer/container/tst_ContainerParser.cpp
)

add_easy_ssh_core_test(tst_ContainerBackends
    explorer/container/tst_ContainerBackends.cpp
)

add_easy_ssh_core_test(tst_SystemdBackend
    explorer/service/tst_SystemdBackend.cpp
)

add_easy_ssh_core_test(tst_ConnectionQuery
    connection/tst_ConnectionQuery.cpp
)

add_easy_ssh_core_test(tst_SshConfigParser
    connection/tst_SshConfigParser.cpp
)

add_easy_ssh_core_test(tst_WorkspaceState
    session/tst_WorkspaceState.cpp
)

add_easy_ssh_core_test(tst_FuzzyMatch
    util/tst_FuzzyMatch.cpp
)

add_easy_ssh_core_test(tst_ShellCommandSet
    fs/tst_ShellCommandSet.cpp
)

add_easy_ssh_core_test(tst_TransferJobStore
    fs/tst_TransferJobStore.cpp
)

add_easy_ssh_core_test(tst_TunnelDefinitionAndStore
    tunnel/tst_TunnelDefinitionAndStore.cpp
)

add_easy_ssh_core_test(tst_Socks5Handshake
    tunnel/tst_Socks5Handshake.cpp
)
