# Read the installed package owning a Konsole executable or library, not a
# repository candidate. Preserve exact source identity separately from the runtime
# dependency, which accepts any RPM release of the same upstream version.
function(cq_query_konsole_rpm rpm path prefix)
    execute_process(COMMAND "${rpm}" -qf "${path}"
        --queryformat "%{NAME}|%{EPOCHNUM}|%{VERSION}|%{RELEASE}|%{ARCH}|%{SOURCERPM}"
        OUTPUT_VARIABLE record ERROR_VARIABLE error RESULT_VARIABLE result
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot query the installed RPM owning ${path}: ${error}${record}")
    endif()
    string(REPLACE "|" ";" fields "${record}")
    list(LENGTH fields count)
    if(NOT count EQUAL 6 OR record MATCHES "[\r\n]")
        message(FATAL_ERROR "Expected exactly one installed Konsole RPM, got: ${record}")
    endif()
    list(GET fields 0 name)
    list(GET fields 1 epoch)
    list(GET fields 2 version)
    list(GET fields 3 release)
    list(GET fields 4 arch)
    list(GET fields 5 source_rpm)
    execute_process(COMMAND "${rpm}" --eval "%{_arch}|%{_isa}"
        OUTPUT_VARIABLE platform RESULT_VARIABLE result OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0 OR NOT platform MATCHES "^${arch}\\|(\\([A-Za-z0-9_-]+\\))$")
        message(FATAL_ERROR "Native RPM architecture must match Konsole's ${arch}: ${platform}")
    endif()
    set(isa "${CMAKE_MATCH_1}")
    set(${prefix}_SOURCE_RPM "${source_rpm}" PARENT_SCOPE)
    set(${prefix}_NEVRA "${name}-${epoch}:${version}-${release}.${arch}" PARENT_SCOPE)
    set(${prefix}_VERSION "${version}" PARENT_SCOPE)
    set(${prefix}_ARCH "${arch}" PARENT_SCOPE)
    set(${prefix}_DEPENDENCY "${name}${isa} = ${epoch}:${version}" PARENT_SCOPE)
endfunction()
