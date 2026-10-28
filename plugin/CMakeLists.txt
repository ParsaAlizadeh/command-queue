find_package(KF6 REQUIRED COMPONENTS CoreAddons XmlGui Config I18n)

configure_file(commandqueue.json.in commandqueue.json @ONLY)
add_library(konsole_commandqueue MODULE commandqueueplugin.cpp sessiontracker.cpp sessiontracker.h sessionqueue.cpp sessionqueue.h)
target_include_directories(konsole_commandqueue PRIVATE
    "${KONSOLE_SOURCE_DIR}" "${KONSOLE_SOURCE_DIR}/src"
    "${KONSOLE_SOURCE_DIR}/src/terminalDisplay"
    "${KONSOLE_BINARY_DIR}" "${KONSOLE_BINARY_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(konsole_commandqueue PRIVATE
    Konsole::konsoleapp Konsole::konsoleprivate
    Qt6::Widgets KF6::CoreAddons KF6::XmlGui KF6::ConfigCore KF6::I18n)
set_target_properties(konsole_commandqueue PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/konsoleplugins")

# Install our files only. Installed binaries resolve Konsole's versioned system
# libraries, never the private development build.
set_target_properties(konsole_commandqueue PROPERTIES INSTALL_RPATH "" INSTALL_RPATH_USE_LINK_PATH FALSE)
install(TARGETS konsole_commandqueue LIBRARY DESTINATION "${KDE_INSTALL_QTPLUGINDIR}/konsoleplugins")
install(FILES shell/integration.bash shell/integration.zsh
    DESTINATION "${KDE_INSTALL_DATADIR}/konsole-command-queue/shell")
configure_file(build-info.json.in build-info.json @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/build-info.json"
    DESTINATION "${KDE_INSTALL_DATADIR}/konsole-command-queue")

message(STATUS "Konsole version: ${KONSOLE_VERSION}")
message(STATUS "Konsole source headers: ${KONSOLE_SOURCE_DIR}/src")
message(STATUS "Konsole generated headers: ${KONSOLE_BINARY_DIR}/src")
message(STATUS "Plugin search root: ${CMAKE_BINARY_DIR}/plugins")

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(toggle_test tests/toggle_test.cpp)
    target_compile_definitions(toggle_test PRIVATE EXPECTED_KONSOLE_VERSION="${KONSOLE_VERSION}" EXPECTED_PLUGIN_VERSION="${PROJECT_VERSION}" EXPECTED_KONSOLE_SOURCE_RPM="${KONSOLE_SOURCE_RPM}")
    target_compile_definitions(toggle_test PRIVATE PLUGIN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(toggle_test PRIVATE
        $<TARGET_PROPERTY:konsole_commandqueue,INCLUDE_DIRECTORIES>)
    target_link_libraries(toggle_test PRIVATE
        Konsole::konsoleapp Konsole::konsoleprivate
        Qt6::Test Qt6::Widgets KF6::CoreAddons KF6::XmlGui KF6::ConfigCore KF6::I18n)
    add_dependencies(toggle_test konsole_commandqueue)
    add_test(NAME toggle COMMAND toggle_test)
    set_tests_properties(toggle PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins;XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/test-state/config;XDG_DATA_HOME=${CMAKE_BINARY_DIR}/test-state/data;XDG_CACHE_HOME=${CMAKE_BINARY_DIR}/test-state/cache")
endif()
