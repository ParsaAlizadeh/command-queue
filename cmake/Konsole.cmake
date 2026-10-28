include(FetchContent)
include(ExternalProject)

set(CQ_KONSOLE_VERSION "" CACHE STRING "Exact Konsole release (empty: detect installed Konsole)")
find_program(CQ_KONSOLE_EXECUTABLE konsole DOC "Installed Konsole executable")
if(CQ_KONSOLE_VERSION)
    set(KONSOLE_VERSION "${CQ_KONSOLE_VERSION}")
elseif(CQ_KONSOLE_EXECUTABLE)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
        "${CQ_KONSOLE_EXECUTABLE}" --version
        OUTPUT_VARIABLE version_output RESULT_VARIABLE version_result OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT version_result EQUAL 0 OR NOT version_output MATCHES "konsole ([0-9]+\\.[0-9]+\\.[0-9]+)")
        message(FATAL_ERROR "Cannot determine Konsole version: ${version_output}")
    endif()
    set(KONSOLE_VERSION "${CMAKE_MATCH_1}")
else()
    message(FATAL_ERROR "Install Konsole, or specify -DCQ_KONSOLE_VERSION=X.Y.Z for a packaging build.")
endif()
if(NOT KONSOLE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "CQ_KONSOLE_VERSION must be an exact release, e.g. 26.08.1.")
endif()

# Fetch only: Konsole expects to be its own top-level CMake project.
# A versioned content name keeps different releases' source/build trees apart.
string(REPLACE "." "_" content_name "konsole_${KONSOLE_VERSION}")
FetchContent_Declare(${content_name}
    GIT_REPOSITORY https://invent.kde.org/utilities/konsole.git
    GIT_TAG "v${KONSOLE_VERSION}"
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
    GIT_SUBMODULES ""
    SOURCE_SUBDIR command-queue-fetch-only)
FetchContent_MakeAvailable(${content_name})
set(KONSOLE_SOURCE_DIR "${${content_name}_SOURCE_DIR}")
set(KONSOLE_BINARY_DIR "${${content_name}_BINARY_DIR}")
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${KONSOLE_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE KONSOLE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)

# Generate the private headers at configure time. Compilation is a normal
# dependency of the plugin; Konsole's install rules are never included in ours.
execute_process(COMMAND "${CMAKE_COMMAND}"
    -S "${KONSOLE_SOURCE_DIR}" -B "${KONSOLE_BINARY_DIR}" -G "${CMAKE_GENERATOR}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}"
    "-DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}"
    "-DCMAKE_INSTALL_PREFIX=${KONSOLE_BINARY_DIR}/install"
    -DBUILD_TESTING=OFF -DENABLE_PLUGIN_SSHMANAGER=OFF -DENABLE_PLUGIN_QUICKCOMMANDS=OFF
    -DKDE_SKIP_UNINSTALL_TARGET=ON
    COMMAND_ERROR_IS_FATAL ANY)
load_cache("${KONSOLE_BINARY_DIR}" READ_WITH_PREFIX HOST_ CMAKE_PROJECT_VERSION)
if(NOT HOST_CMAKE_PROJECT_VERSION STREQUAL KONSOLE_VERSION)
    message(FATAL_ERROR "Fetched Konsole version ${HOST_CMAKE_PROJECT_VERSION} differs from ${KONSOLE_VERSION}.")
endif()

# Bound the isolated build even when an outer Ninja build uses its own defaults.
set(CQ_KONSOLE_JOBS 2 CACHE STRING "Parallel jobs for the isolated Konsole build")
if(NOT CQ_KONSOLE_JOBS MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "CQ_KONSOLE_JOBS must be a positive integer.")
endif()
set(konsole_libraries)
foreach(component konsoleapp konsoleprivate)
    set(library "${KONSOLE_BINARY_DIR}/bin/${CMAKE_SHARED_LIBRARY_PREFIX}${component}${CMAKE_SHARED_LIBRARY_SUFFIX}.${KONSOLE_VERSION}")
    list(APPEND konsole_libraries "${library}")
    add_library(Konsole::${component} SHARED IMPORTED GLOBAL)
    set_target_properties(Konsole::${component} PROPERTIES IMPORTED_LOCATION "${library}")
endforeach()
ExternalProject_Add(konsole_dependency
    SOURCE_DIR "${KONSOLE_SOURCE_DIR}"
    BINARY_DIR "${KONSOLE_BINARY_DIR}"
    DOWNLOAD_COMMAND "" UPDATE_COMMAND "" PATCH_COMMAND "" CONFIGURE_COMMAND ""
    BUILD_COMMAND "${CMAKE_COMMAND}" --build "${KONSOLE_BINARY_DIR}" --target konsoleapp --parallel "${CQ_KONSOLE_JOBS}"
    BUILD_ALWAYS TRUE
    BUILD_BYPRODUCTS ${konsole_libraries}
    INSTALL_COMMAND ""
    USES_TERMINAL_BUILD TRUE)
foreach(component konsoleapp konsoleprivate)
    add_dependencies(Konsole::${component} konsole_dependency)
endforeach()
# Optional development host. The default build only needs the libraries.
add_custom_target(konsole_host
    COMMAND "${CMAKE_COMMAND}" --build "${KONSOLE_BINARY_DIR}" --target konsole --parallel "${CQ_KONSOLE_JOBS}"
    DEPENDS konsole_dependency USES_TERMINAL)
string(REPLACE "'" "'\\''" shell_source_dir "${KONSOLE_SOURCE_DIR}")
string(REPLACE "'" "'\\''" shell_binary_dir "${KONSOLE_BINARY_DIR}")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/konsole-paths.sh" CONTENT
    "export KONSOLE_SOURCE_DIR='${shell_source_dir}'\nexport KONSOLE_BINARY_DIR='${shell_binary_dir}'\n")
message(STATUS "Command Queue ${PROJECT_VERSION}, Konsole ${KONSOLE_VERSION} (${KONSOLE_REVISION})")
