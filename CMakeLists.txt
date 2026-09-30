cmake_minimum_required(VERSION 3.15)
project(KokoroCPP VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# libkokoro only exports the C API; everything else stays internal.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if (MSVC)
    add_compile_options("/utf-8" "/EHsc" "/Zc:inline" "/permissive-" "/Ob2")
    add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ONNX Runtime Configuration
# Priority 1: third_party/onnxruntime, downloaded by `uv run bootstrap.py configure`
set(LOCAL_ONNX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime")
if(EXISTS "${LOCAL_ONNX_PATH}")
    message(STATUS "Using local ONNX Runtime from: ${LOCAL_ONNX_PATH}")
    set(ONNXRUNTIME_ROOT "${LOCAL_ONNX_PATH}")
# Priority 2: User defined ONNXRUNTIME_ROOT
elseif(NOT DEFINED ONNXRUNTIME_ROOT)
    # Priority 3: Common system paths
    if (APPLE)
        set(ONNXRUNTIME_ROOT "/opt/homebrew/opt/onnxruntime")
    else()
        set(ONNXRUNTIME_ROOT "/usr/local")
    endif()
endif()

message(STATUS "Looking for ONNX Runtime in: ${ONNXRUNTIME_ROOT}")

# Forget cached results that no longer exist (e.g. after the bundled runtime was replaced).
foreach(var ONNXRUNTIME_INCLUDE_DIR ONNXRUNTIME_LIB ONNXRUNTIME_DLL)
    if (${var} AND NOT EXISTS "${${var}}")
        unset(${var} CACHE)
    endif()
endforeach()

find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
    HINTS ${ONNXRUNTIME_ROOT}/include
    PATH_SUFFIXES onnxruntime core/session
)

find_library(ONNXRUNTIME_LIB onnxruntime
    HINTS ${ONNXRUNTIME_ROOT}/lib
)

if (NOT (ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIB))
    message(FATAL_ERROR "ONNX Runtime not found. Run `uv run bootstrap.py configure`, or set ONNXRUNTIME_ROOT, e.g. cmake -DONNXRUNTIME_ROOT=/path/to/onnxruntime ..")
endif()
message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIB}")

# Windows has no rpath: onnxruntime.dll has to sit next to every executable that loads kokoro.dll.
if (WIN32)
    get_filename_component(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_LIB}" DIRECTORY)
    find_file(ONNXRUNTIME_DLL onnxruntime.dll
        HINTS ${ONNXRUNTIME_LIB_DIR} ${ONNXRUNTIME_ROOT}/bin
        NO_DEFAULT_PATH
    )
    if (NOT ONNXRUNTIME_DLL)
        message(FATAL_ERROR "onnxruntime.dll not found next to ${ONNXRUNTIME_LIB}")
    endif()
endif()

# Internal implementation, linked into libkokoro and test_g2p.
add_library(kokoro_core STATIC
    src/Kokoro.cpp
    src/Tokenizer.cpp
    src/ZHG2P.cpp
    src/PinyinFinder.cpp
    src/ToneSandhi.cpp
    src/ZHFrontend.cpp
    src/NeuralG2P.cpp
)
target_include_directories(kokoro_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppjieba/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppjieba/deps/limonp/include
    ${ONNXRUNTIME_INCLUDE_DIR}
)

# Public shared library: C API from include/kokoro/kokoro.h.
add_library(kokoro SHARED src/kokoro_c.cpp)
add_library(kokoro::kokoro ALIAS kokoro)
target_include_directories(kokoro PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(kokoro PRIVATE KOKORO_BUILD KOKORO_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(kokoro PRIVATE kokoro_core ${ONNXRUNTIME_LIB})
set_target_properties(kokoro PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if (WIN32)
    add_custom_command(TARGET kokoro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ONNXRUNTIME_DLL}" $<TARGET_FILE_DIR:kokoro>)
    install(FILES "${ONNXRUNTIME_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})
elseif (APPLE)
    target_link_libraries(kokoro PRIVATE "-framework Foundation" "-framework CoreFoundation")
    set_target_properties(kokoro PROPERTIES INSTALL_RPATH "@loader_path")
elseif (UNIX)
    set_target_properties(kokoro PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

# Command-line tool. Target name differs from the library; the executable is still named `kokoro`.
add_executable(kokoro_cli src/cli/main.cpp)
target_link_libraries(kokoro_cli PRIVATE kokoro)
set_target_properties(kokoro_cli PROPERTIES OUTPUT_NAME kokoro)
if (WIN32)
    target_link_libraries(kokoro_cli PRIVATE Shell32)   # CommandLineToArgvW
endif()
if (MSVC)
    # kokoro.exe and kokoro.dll share an output directory: keep their .pdb/.ilk apart.
    set_target_properties(kokoro_cli PROPERTIES PDB_NAME kokoro_cli)
    target_link_options(kokoro_cli PRIVATE "/INCREMENTAL:NO")
elseif (APPLE)
    set_target_properties(kokoro_cli PROPERTIES INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
elseif (UNIX)
    set_target_properties(kokoro_cli PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

# Install: headers, libkokoro and a CMake package (find_package(kokoro) -> kokoro::kokoro).
install(TARGETS kokoro EXPORT kokoroTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(TARGETS kokoro_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/kokoro DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT kokoroTargets NAMESPACE kokoro:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kokoro)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/kokoroConfig.cmake
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/kokoroTargets.cmake\")\n")
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/kokoroConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kokoroConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/kokoroConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kokoro
)

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(KOKORO_IS_TOP_LEVEL ON)
else()
    set(KOKORO_IS_TOP_LEVEL OFF)
endif()
option(KOKORO_BUILD_TESTS "Build the kokoro tests" ${KOKORO_IS_TOP_LEVEL})
if (KOKORO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

