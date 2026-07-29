find_package(Qt6 6.6 REQUIRED COMPONENTS Core Network Test)

set(EASY_SSH_IT_SOURCES
    harness/integration_tests.cpp
    harness/SshTestHarness.cpp
    ${CMAKE_SOURCE_DIR}/src/SshWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/Logging.cpp
)

add_executable(easy-ssh-integration-tests ${EASY_SSH_IT_SOURCES})

target_include_directories(easy-ssh-integration-tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/harness
)

target_link_libraries(easy-ssh-integration-tests
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Test
        ssh
)

add_test(NAME integration-tests COMMAND easy-ssh-integration-tests)
set_tests_properties(integration-tests PROPERTIES
    LABELS "integration"
    TIMEOUT 120
    ENVIRONMENT "EASY_SSH_IT_DIRECT_HOST=127.0.0.1;EASY_SSH_IT_DIRECT_PORT=12222"
)

message(STATUS "Integration tests: easy-ssh-integration-tests (headless SshWorker)")
