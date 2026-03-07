cmake_minimum_required(VERSION 3.19)
project(nymea-cli VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network)
find_package(Python3 COMPONENTS Interpreter)

include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

qt_standard_project_setup()

set(NYMEA_API_JSON "${CMAKE_SOURCE_DIR}/api/api.json" CACHE FILEPATH "Path to nymea api.json")
set(NYMEA_API_GENERATOR "${CMAKE_SOURCE_DIR}/scripts/generate_nymea_api.py")
set(NYMEA_API_OUTPUT "${CMAKE_SOURCE_DIR}/src/generated/nymeaapigenerated.h")

if (Python3_Interpreter_FOUND)
    add_custom_target(generate-nymea-api
        COMMAND "${Python3_EXECUTABLE}" "${NYMEA_API_GENERATOR}" --api-json "${NYMEA_API_JSON}" --output "${NYMEA_API_OUTPUT}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        BYPRODUCTS "${NYMEA_API_OUTPUT}"
        COMMENT "Generating nymea Qt/C++ API model from ${NYMEA_API_JSON}"
        VERBATIM
    )
endif()

qt_add_executable(nymea-cli
    main.cpp
    src/connectionsettings.cpp
    src/connectionsettings.h
    src/engine.cpp
    src/engine.h
    src/nymeajsonrpcclient.cpp
    src/nymeajsonrpcclient.h
    src/thingmanager.cpp
    src/thingmanager.h
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
        Qt::Core
        Qt::Network
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
