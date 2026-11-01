include("${CMAKE_CURRENT_LIST_DIR}/DownloadKonsoleSource.cmake")
set(source_rpm konsole-26.08.2-1.fc44.src.rpm)
if(DEFINED DOWNLOAD_CASE)
    set(ENV{CQ_DOWNLOAD_CASE} "${DOWNLOAD_CASE}")
    set(ENV{CQ_DOWNLOAD_TEST_DIR} "${TEST_BINARY_DIR}")
    cq_download_konsole_source("${TEST_BINARY_DIR}/fake-dnf" "${source_rpm}" "${TEST_BINARY_DIR}/source")
    return()
endif()

# Exercise retry behavior without network access or changes to host repositories.
set(fake_dnf [=[#!/usr/bin/env bash
set -eu
printf '%s\n' "$*" >> "$CQ_DOWNLOAD_TEST_DIR/calls"
for required in --assumeno --repo=fedora-source --repo=updates-source; do
    found=false
    for arg in "$@"; do
        if [[ $arg == "$required" ]]; then found=true; fi
    done
    if [[ $found == false ]]; then exit 90; fi
done
case " $* " in
    *' clean metadata '*)
        if [[ $CQ_DOWNLOAD_CASE == clean_failure ]]; then
            echo 'metadata cleanup failed' >&2
            exit 8
        fi
        touch "$CQ_DOWNLOAD_TEST_DIR/cleaned"
        exit 0
        ;;
esac
destdir=
for arg in "$@"; do
    case $arg in --destdir=*) destdir=${arg#--destdir=} ;; esac
done
[[ -n $destdir && ${!#} == konsole-26.08.2-1.fc44.src ]] || exit 91
archive="$destdir/konsole-26.08.2-1.fc44.src.rpm"
case $CQ_DOWNLOAD_CASE in
    missing|clean_failure) exit 0 ;;
    failure)
        echo partial > "$archive"
        echo 'download failed' >&2
        exit 7
        ;;
    stale|partial)
        if [[ ! -e $CQ_DOWNLOAD_TEST_DIR/cleaned ]]; then
            if [[ $CQ_DOWNLOAD_CASE == partial ]]; then
                echo partial > "$archive"
                exit 7
            fi
            exit 0
        fi
        [[ ! -e $archive ]] || exit 92
        ;;
esac
echo complete > "$archive"
]=])
# Keep every case isolated, including DNF's metadata directory.
foreach(case success cached stale partial missing failure clean_failure)
    set(case_dir "${TEST_BINARY_DIR}/${case}")
    file(REMOVE_RECURSE "${case_dir}")
    file(MAKE_DIRECTORY "${case_dir}/source")
    file(WRITE "${case_dir}/fake-dnf" "${fake_dnf}")
    file(CHMOD "${case_dir}/fake-dnf" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    if(case STREQUAL "cached")
        file(WRITE "${case_dir}/source/${source_rpm}" "cached")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DDOWNLOAD_CASE=${case}"
        "-DTEST_BINARY_DIR=${case_dir}" -P "${CMAKE_CURRENT_LIST_FILE}"
        WORKING_DIRECTORY "${case_dir}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(calls "")
    if(EXISTS "${case_dir}/calls")
        file(STRINGS "${case_dir}/calls" calls)
    endif()
    list(LENGTH calls count)
    if(case STREQUAL "cached")
        set(expected_calls 0)
    elseif(case STREQUAL "success")
        set(expected_calls 1)
    elseif(case STREQUAL "clean_failure")
        set(expected_calls 2)
    else()
        set(expected_calls 3)
    endif()
    if(NOT count EQUAL expected_calls)
        message(FATAL_ERROR "${case}: expected ${expected_calls} DNF calls, got ${count}: ${output}${error}")
    endif()
    if(case MATCHES "^(missing|failure|clean_failure)$")
        if(result STREQUAL "0" OR EXISTS "${case_dir}/source/${source_rpm}")
            message(FATAL_ERROR "${case}: failure accepted or partial archive retained")
        endif()
        file(READ "${case_dir}/source/download.log" log)
        if(case STREQUAL "missing" AND NOT log MATCHES "success but did not produce")
            message(FATAL_ERROR "Missing output was not recorded: ${log}")
        elseif(case STREQUAL "failure" AND (NOT log MATCHES "exit status: 7" OR NOT log MATCHES "download failed"))
            message(FATAL_ERROR "Download failure details were not recorded: ${log}")
        elseif(case STREQUAL "clean_failure" AND NOT log MATCHES "Metadata cleanup, exit status: 8")
            message(FATAL_ERROR "Cleanup failure details were not recorded: ${log}")
        endif()
    elseif(NOT result STREQUAL "0" OR NOT EXISTS "${case_dir}/source/${source_rpm}")
        message(FATAL_ERROR "${case}: expected success: ${output}${error}")
    endif()
endforeach()
