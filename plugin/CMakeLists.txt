cmake_minimum_required(VERSION 3.16)
project(konsole_commandqueue VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Konsole does not install a public plugin SDK. Supply both its original
# headers and the matching generated headers/libraries from a configured build.
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(KONSOLE_SOURCE_DIR "${PROJECT_ROOT}/vendor/konsole" CACHE PATH "Matching Konsole source tree")
set(KONSOLE_BINARY_DIR "${PROJECT_ROOT}/build/konsole" CACHE PATH "Configured and built Konsole tree")
if(NOT EXISTS "${KONSOLE_BINARY_DIR}/CMakeCache.txt")
    message(FATAL_ERROR "Configure Konsole first, or set KONSOLE_BINARY_DIR.")
endif()
load_cache("${KONSOLE_BINARY_DIR}" READ_WITH_PREFIX HOST_
    CMAKE_HOME_DIRECTORY CMAKE_PROJECT_VERSION)
get_filename_component(HOST_SOURCE "${HOST_CMAKE_HOME_DIRECTORY}" REALPATH)
get_filename_component(REQUESTED_SOURCE "${KONSOLE_SOURCE_DIR}" REALPATH)
if(NOT HOST_SOURCE STREQUAL REQUESTED_SOURCE)
    message(FATAL_ERROR "KONSOLE_SOURCE_DIR does not match the source used by KONSOLE_BINARY_DIR.")
endif()
set(KONSOLE_VERSION "${HOST_CMAKE_PROJECT_VERSION}")
if(NOT KONSOLE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "Cannot read the Konsole version from its CMake cache.")
endif()
foreach(header src/config-konsole.h src/konsoleapp_export.h src/konsoleprivate_export.h)
    if(NOT EXISTS "${KONSOLE_BINARY_DIR}/${header}")
        message(FATAL_ERROR "Missing generated Konsole header: ${header}")
    endif()
endforeach()

find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
include(KDEInstallDirs)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6 REQUIRED COMPONENTS CoreAddons XmlGui Config I18n)

foreach(component konsoleapp konsoleprivate)
    find_library(${component}_LIBRARY NAMES ${component}
        PATHS "${KONSOLE_BINARY_DIR}/bin" NO_DEFAULT_PATH REQUIRED)
    add_library(Konsole::${component} SHARED IMPORTED)
    set_target_properties(Konsole::${component} PROPERTIES
        IMPORTED_LOCATION "${${component}_LIBRARY}")
endforeach()

configure_file(commandqueue.json.in commandqueue.json @ONLY)
add_library(konsole_commandqueue MODULE commandqueueplugin.cpp)
target_include_directories(konsole_commandqueue PRIVATE
    "${KONSOLE_SOURCE_DIR}" "${KONSOLE_SOURCE_DIR}/src"
    "${KONSOLE_SOURCE_DIR}/src/terminalDisplay"
    "${KONSOLE_BINARY_DIR}/src/characters"
    "${KONSOLE_BINARY_DIR}" "${KONSOLE_BINARY_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(konsole_commandqueue PRIVATE
    Konsole::konsoleapp Konsole::konsoleprivate
    Qt6::Widgets KF6::CoreAddons KF6::XmlGui KF6::ConfigCore KF6::I18n)
set_target_properties(konsole_commandqueue PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins/konsoleplugins")

message(STATUS "Konsole version: ${KONSOLE_VERSION}")
message(STATUS "Konsole source headers: ${KONSOLE_SOURCE_DIR}/src")
message(STATUS "Konsole generated headers: ${KONSOLE_BINARY_DIR}/src")
message(STATUS "Plugin search root: ${CMAKE_CURRENT_BINARY_DIR}/plugins")

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(toggle_test tests/toggle_test.cpp)
    target_compile_definitions(toggle_test PRIVATE EXPECTED_KONSOLE_VERSION="${KONSOLE_VERSION}")
    target_include_directories(toggle_test PRIVATE
        $<TARGET_PROPERTY:konsole_commandqueue,INCLUDE_DIRECTORIES>)
    target_link_libraries(toggle_test PRIVATE
        Konsole::konsoleapp Konsole::konsoleprivate
        Qt6::Test Qt6::Widgets KF6::CoreAddons KF6::XmlGui KF6::ConfigCore KF6::I18n)
    add_dependencies(toggle_test konsole_commandqueue)
    add_test(NAME toggle COMMAND toggle_test)
    set_tests_properties(toggle PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${CMAKE_CURRENT_BINARY_DIR}/plugins;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-state/config;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-state/data;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-state/cache")
endif()
