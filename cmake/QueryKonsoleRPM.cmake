# Read the package owning the selected Konsole executable, not a repository's
# candidate version. Epoch and distribution release are part of the ABI pin.
function(cq_query_konsole_rpm rpm executable prefix)
    execute_process(COMMAND "${rpm}" -qf "${executable}"
        --queryformat "%{NAME}|%{EPOCHNUM}|%{VERSION}|%{RELEASE}|%{ARCH}"
        OUTPUT_VARIABLE record ERROR_VARIABLE error RESULT_VARIABLE result
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot query the installed RPM owning ${executable}: ${error}${record}")
    endif()
    string(REPLACE "|" ";" fields "${record}")
    list(LENGTH fields count)
    if(NOT count EQUAL 5 OR record MATCHES "[\r\n]")
        message(FATAL_ERROR "Expected exactly one installed Konsole RPM, got: ${record}")
    endif()
    list(GET fields 0 name)
    list(GET fields 1 epoch)
    list(GET fields 2 version)
    list(GET fields 3 release)
    list(GET fields 4 arch)
    execute_process(COMMAND "${rpm}" --eval "%{_arch}|%{_isa}"
        OUTPUT_VARIABLE platform RESULT_VARIABLE result OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0 OR NOT platform MATCHES "^${arch}\\|(\\([A-Za-z0-9_-]+\\))$")
        message(FATAL_ERROR "Native RPM architecture must match Konsole's ${arch}: ${platform}")
    endif()
    set(isa "${CMAKE_MATCH_1}")
    set(${prefix}_VERSION "${version}" PARENT_SCOPE)
    set(${prefix}_RELEASE "${release}" PARENT_SCOPE)
    set(${prefix}_EPOCH "${epoch}" PARENT_SCOPE)
    set(${prefix}_ARCH "${arch}" PARENT_SCOPE)
    set(${prefix}_DEPENDENCY "${name}${isa} = ${epoch}:${version}-${release}" PARENT_SCOPE)
endfunction()
