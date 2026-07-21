cmake_minimum_required(VERSION 3.19)
project(nymea-cli VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(FTXUI_GIT_TAG "v6.1.9")
option(NYMEA_USE_SYSTEM_FTXUI "Prefer a system-installed FTXUI package over FetchContent." ON)
option(NYMEA_ALLOW_FETCHCONTENT_FTXUI "Allow downloading FTXUI with FetchContent when no system package is available." ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)
find_package(Python3 COMPONENTS Interpreter)

set(NYMEA_FTXUI_VERSION "${FTXUI_GIT_TAG}")
if (NYMEA_USE_SYSTEM_FTXUI)
    find_package(ftxui CONFIG QUIET)
    if (TARGET ftxui::component AND TARGET ftxui::dom AND TARGET ftxui::screen)
        if (DEFINED ftxui_VERSION AND NOT "${ftxui_VERSION}" STREQUAL "")
            set(NYMEA_FTXUI_VERSION "${ftxui_VERSION}")
        endif()
    endif()
endif()

if (NOT (TARGET ftxui::component AND TARGET ftxui::dom AND TARGET ftxui::screen))
    if (NOT NYMEA_ALLOW_FETCHCONTENT_FTXUI)
        message(FATAL_ERROR "FTXUI was not found as a system package and NYMEA_ALLOW_FETCHCONTENT_FTXUI is OFF.")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG ${FTXUI_GIT_TAG}
    )
    set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "Disable FTXUI install rules when vendored by nymea-cli" FORCE)
    FetchContent_MakeAvailable(ftxui)
endif()

if (NOT (TARGET ftxui::component AND TARGET ftxui::dom AND TARGET ftxui::screen))
    message(FATAL_ERROR "FTXUI targets are unavailable. Install a system ftxui package or allow FetchContent to download it.")
endif()

set(NYMEA_API_JSON "${CMAKE_SOURCE_DIR}/api/api.json" CACHE FILEPATH "Path to nymea api.json")
set(NYMEA_API_GENERATOR "${CMAKE_SOURCE_DIR}/scripts/generate_nymea_api.py")
set(NYMEA_API_OUTPUT "${CMAKE_SOURCE_DIR}/src/generated/nymeaapigenerated.h")
set(NYMEA_API_SOURCE "${CMAKE_SOURCE_DIR}/src/generated/nymeaapigenerated.cpp")

if (Python3_Interpreter_FOUND)
    # Regenerate only when the API definition or the generator itself changes,
    # so ordinary builds don't rewrite the generated sources (and force a full rebuild).
    add_custom_command(
        OUTPUT "${NYMEA_API_OUTPUT}" "${NYMEA_API_SOURCE}"
        COMMAND "${Python3_EXECUTABLE}" "${NYMEA_API_GENERATOR}" --api-json "${NYMEA_API_JSON}" --output "${NYMEA_API_OUTPUT}"
        DEPENDS "${NYMEA_API_JSON}" "${NYMEA_API_GENERATOR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Generating nymea Qt/C++ API model from ${NYMEA_API_JSON}"
        VERBATIM
    )
endif()

add_executable(nymea-cli
    main.cpp
    src/connectionsettings.cpp
    src/connectionsettings.h
    src/engine.cpp
    src/engine.h
    src/engineinternal.h
    src/engineutils.cpp
    src/engineutils.h
    src/enginebrowse.cpp
    src/engineapibrowser.h
    src/enginehelpers.h
    src/engine_connection.cpp
    src/engine_notifications.cpp
    src/engine_things.cpp
    src/engine_configure.cpp
    src/engine_logview.cpp
    src/engine_settings.cpp
    src/engine_serverinterfaces.cpp
    src/engine_modbus.cpp
    src/engine_events.cpp
    src/nymeajsonrpcclient.cpp
    src/nymeajsonrpcclient.h
    src/thingmanager.cpp
    src/thingmanager.h
    src/generated/nymeaapigenerated.cpp
    src/generated/nymeaapigenerated.h
)

target_include_directories(nymea-cli
    PRIVATE
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(nymea-cli
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        ftxui::component
        ftxui::dom
        ftxui::screen
)

target_compile_definitions(nymea-cli
    PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
        APP_LICENSE_SPDX="GPL-3.0-or-later"
        FTXUI_VERSION="${NYMEA_FTXUI_VERSION}"
)

include(GNUInstallDirs)

install(TARGETS nymea-cli
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES docs/nymea-cli.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
)

install(FILES completions/nymea-cli
    DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
)

if (QT_VERSION_MAJOR EQUAL 6 AND COMMAND qt_finalize_executable)
    qt_finalize_executable(nymea-cli)
endif()
