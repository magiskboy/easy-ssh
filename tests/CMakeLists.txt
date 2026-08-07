include(CTest)

enable_testing()

find_package(Qt6 ${EASY_SSH_QT_VERSION} REQUIRED COMPONENTS Test)

add_subdirectory(core)
