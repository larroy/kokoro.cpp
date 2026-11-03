# Maps the CMake target (not the host) to a Rust target triple for kokoro-server.

# MSVC ABI: cl, clang-cl, and GNU-frontend clang++ targeting *-windows-msvc (where CMake leaves MSVC false).
function(_kokoro_msvc_abi out_var)
    if (MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Rust arch for the target, or "" when unknown or when several macOS architectures are requested.
function(_kokoro_rust_arch out_var)
    _kokoro_msvc_abi(msvc_abi)
    if (msvc_abi)
        set(arch_id "${MSVC_CXX_ARCHITECTURE_ID}")
    elseif (APPLE AND CMAKE_OSX_ARCHITECTURES)
        set(arch_id "${CMAKE_OSX_ARCHITECTURES}")
    else()
        set(arch_id "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    string(TOLOWER "${arch_id}" arch_id)
    if (arch_id MATCHES "^(x64|x86_64|amd64)$")
        set(arch x86_64)
    elseif (arch_id MATCHES "^(arm64|aarch64)$")
        set(arch aarch64)
    elseif (arch_id MATCHES "^(x86|i[3-6]86)$")
        set(arch i686)
    else()
        set(arch "")
    endif()
    set(${out_var} "${arch}" PARENT_SCOPE)
endfunction()

# Rust vendor-os(-env) suffix for the target, or "" when unsupported (Android, iOS, BSDs, ...).
function(_kokoro_rust_os out_var)
    _kokoro_msvc_abi(msvc_abi)
    if (WIN32 AND msvc_abi)
        set(os pc-windows-msvc)
    elseif (WIN32 AND MINGW)
        set(os pc-windows-gnu)
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(os apple-darwin)
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(os unknown-linux-gnu)
    else()
        set(os "")
    endif()
    set(${out_var} "${os}" PARENT_SCOPE)
endfunction()

# Sets out_var to the Rust triple for the CMake target, or "" when there is no mapping.
function(kokoro_rust_target out_var)
    _kokoro_rust_arch(arch)
    _kokoro_rust_os(os)
    if (arch AND os)
        set(${out_var} "${arch}-${os}" PARENT_SCOPE)
    else()
        set(${out_var} "" PARENT_SCOPE)
    endif()
endfunction()

# Fails configuration unless the rustc next to cargo has the standard library for the triple.
function(kokoro_require_rust_std cargo triple)
    get_filename_component(cargo_dir "${cargo}" DIRECTORY)
    find_program(KOKORO_RUSTC rustc HINTS "${cargo_dir}")
    execute_process(COMMAND "${KOKORO_RUSTC}" --print target-libdir --target "${triple}"
        OUTPUT_VARIABLE libdir OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE status ERROR_QUIET)
    if (NOT status EQUAL 0 OR NOT IS_DIRECTORY "${libdir}")
        message(FATAL_ERROR "Rust has no standard library for ${triple}: run `rustup target add ${triple}`, "
            "set -DKOKORO_RUST_TARGET=<triple>, or configure with -DKOKORO_BUILD_SERVER=OFF")
    endif()
endfunction()
