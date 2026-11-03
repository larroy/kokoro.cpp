# Stage kokoro.dll and the ONNX Runtime DLLs beside the executables cargo built for `cargo test`.
#
# Windows has no rpath, and cargo no longer keeps test executables in one predictable directory: rustc <= 1.99 wrote
# them to <target>/<profile>/deps, newer cargo writes them to <target>/<profile>/build/<package>/<hash>/out. Ask cargo
# with --message-format=json where they are, then copy the DLLs into the application directory of each one, which the
# loader searches before System32.
#
# Parameters: CARGO (cargo executable), MANIFEST (Cargo.toml of the crate), PROFILE (cargo profile name), TARGET (the
# Rust triple CMake builds for; empty for an explicit host build), DLLS (the DLLs to stage, '|'-separated because a test
# command line cannot carry a ';' list). The caller provides the build environment (CARGO_TARGET_DIR, KOKORO_LIB_DIR,
# ...).
cmake_minimum_required(VERSION 3.15)

foreach(var CARGO MANIFEST PROFILE DLLS)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "stage_test_dlls.cmake: ${var} is required")
    endif()
endforeach()
string(REPLACE "|" ";" dlls "${DLLS}")
# Stage beside the same triple the `server` test runs, not the host binaries cargo builds without --target.
set(target_args "")
if (DEFINED TARGET AND NOT "${TARGET}" STREQUAL "")
    set(target_args --target "${TARGET}")
endif()

execute_process(
    COMMAND "${CARGO}" test --locked --profile "${PROFILE}" --no-run --message-format=json ${target_args}
            --manifest-path "${MANIFEST}"
    OUTPUT_VARIABLE cargo_json
    ERROR_VARIABLE cargo_log
    RESULT_VARIABLE build_rc)
if (build_rc)
    message(FATAL_ERROR "cargo test --no-run failed (${build_rc}):\n${cargo_log}")
endif()

# Each line holds one JSON object; only executable artifacts carry an "executable" field.
string(REGEX MATCHALL "\"executable\":\"[^\"]+\"" executables "${cargo_json}")
set(test_dirs "")
foreach(entry IN LISTS executables)
    string(REGEX REPLACE "^\"executable\":\"([^\"]+)\"$" "\\1" exe "${entry}")
    string(REPLACE "\\\\" "\\" exe "${exe}")   # undo the JSON escaping of Windows separators
    get_filename_component(dir "${exe}" DIRECTORY)
    list(APPEND test_dirs "${dir}")
endforeach()
list(REMOVE_DUPLICATES test_dirs)
if (NOT test_dirs)
    message(FATAL_ERROR "cargo reported no test executables for ${MANIFEST}")
endif()

foreach(dir IN LISTS test_dirs)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${dlls} "${dir}"
                    RESULT_VARIABLE copy_rc ERROR_VARIABLE copy_log)
    if (copy_rc)
        message(FATAL_ERROR "could not stage DLLs in ${dir}: ${copy_log}")
    endif()
    message(STATUS "staged DLLs in ${dir}")
endforeach()
