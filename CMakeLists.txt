cmake_minimum_required(VERSION 3.19)
project(nymea-cli VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)
find_package(Python3 COMPONENTS Interpreter)

include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v6.1.9
)
FetchContent_MakeAvailable(ftxui)

set(NYMEA_API_JSON "${CMAKE_SOURCE_DIR}/api/api.json" CACHE FILEPATH "Path to nymea api.json")
set(NYMEA_API_GENERATOR "${CMAKE_SOURCE_DIR}/scripts/generate_nymea_api.py")
set(NYMEA_API_OUTPUT "${CMAKE_SOURCE_DIR}/src/generated/nymeaapigenerated.h")
set(NYMEA_API_SOURCE "${CMAKE_SOURCE_DIR}/src/generated/nymeaapigenerated.cpp")

if (Python3_Interpreter_FOUND)
    add_custom_target(generate-nymea-api
        COMMAND "${Python3_EXECUTABLE}" "${NYMEA_API_GENERATOR}" --api-json "${NYMEA_API_JSON}" --output "${NYMEA_API_OUTPUT}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        BYPRODUCTS "${NYMEA_API_OUTPUT}" "${NYMEA_API_SOURCE}"
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
    src/nymeajsonrpcclient.cpp
    src/nymeajsonrpcclient.h
    src/thingmanager.cpp
    src/thingmanager.h
    src/generated/nymeaapigenerated.cpp
    src/generated/nymeaapigenerated.h
)

if (TARGET generate-nymea-api)
    add_dependencies(nymea-cli generate-nymea-api)
endif()

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
)

include(GNUInstallDirs)

install(TARGETS nymea-cli
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if (QT_VERSION_MAJOR EQUAL 6 AND COMMAND qt_finalize_executable)
    qt_finalize_executable(nymea-cli)
endif()
