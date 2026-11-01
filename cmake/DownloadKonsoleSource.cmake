# Keep unrelated repositories (and their key-import prompts) out of source
# preparation. All DNF state remains writable by the unprivileged build user.
function(cq_download_konsole_source dnf source_rpm work)
    set(archive "${work}/${source_rpm}")
    if(EXISTS "${archive}")
        return()
    endif()
    file(MAKE_DIRECTORY "${work}")
    set(dnf_command "${dnf}" --assumeno
        --repo=fedora-source --repo=updates-source
        "--setopt=cachedir=${CMAKE_BINARY_DIR}/dnf/cache"
        "--setopt=persistdir=${CMAKE_BINARY_DIR}/dnf/state"
        "--setopt=logdir=${CMAKE_BINARY_DIR}/dnf/log")
    string(REGEX REPLACE "\\.rpm$" "" source_spec "${source_rpm}")
    set(log "${work}/download.log")
    file(WRITE "${log}" "Source RPM: ${source_rpm}\nRepositories: fedora-source, updates-source\n")
    foreach(attempt RANGE 1 2)
        message(STATUS "Downloading installed Konsole source: ${source_rpm} (attempt ${attempt}/2)")
        execute_process(COMMAND ${dnf_command} download --srpm "--destdir=${work}" "${source_spec}"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
        file(APPEND "${log}" "\nDownload attempt ${attempt}, exit status: ${result}\n${output}\n${error}\n")
        if(result STREQUAL "0" AND EXISTS "${archive}")
            return()
        endif()
        if(result STREQUAL "0")
            file(APPEND "${log}" "DNF reported success but did not produce ${source_rpm}.\n")
        endif()
        file(REMOVE "${archive}")
        if(attempt EQUAL 1)
            # A successful DNF invocation can still select no matching source
            # when cached metadata predates the installed Konsole update.
            message(STATUS "Source download failed; invalidating project DNF metadata before retrying")
            execute_process(COMMAND ${dnf_command} clean metadata
                RESULT_VARIABLE clean_result OUTPUT_VARIABLE output ERROR_VARIABLE error)
            file(APPEND "${log}" "\nMetadata cleanup, exit status: ${clean_result}\n${output}\n${error}\n")
            if(NOT clean_result STREQUAL "0")
                message(FATAL_ERROR "Cannot refresh project DNF metadata. See ${log}.")
            endif()
        endif()
    endforeach()
    message(FATAL_ERROR "Cannot download ${source_rpm} from Fedora source repositories after two attempts (last DNF exit status: ${result}). See ${log}. The exact installed source package must be available in fedora-source or updates-source.")
endfunction()
