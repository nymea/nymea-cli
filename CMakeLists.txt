cmake_minimum_required(VERSION 3.19)
project(nymea-cli VERSION 0.1.0 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

qt_standard_project_setup()

qt_add_executable(nymea-cli
    main.cpp
)

target_link_libraries(nymea-cli
    PRIVATE
        Qt::Core
        ftxui::component
        ftxui::dom
        ftxui::screen
)

target_compile_definitions(nymea-cli
    PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
)

include(GNUInstallDirs)

install(TARGETS nymea-cli
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
