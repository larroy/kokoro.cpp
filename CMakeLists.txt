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

    # The bundled static onnxruntime.lib is built with /MT and _ITERATOR_DEBUG_LEVEL=0,
    # so every configuration must use the static release CRT.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

    add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ONNX Runtime Configuration
# Priority 1: Check third_party/onnxruntime
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

find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
    HINTS ${ONNXRUNTIME_ROOT}/include
    PATH_SUFFIXES onnxruntime core/session
)

# 查找静态链接库
# 首先尝试查找静态库
find_library(ONNXRUNTIME_LIB onnxruntime
    HINTS ${ONNXRUNTIME_ROOT}/lib
    PATH_SUFFIXES 
        mac_arm64 
        linux_x64 
        windows_x64
        mac_x64
        linux_aarch64
)

# 如果没有找到静态库，尝试查找并解压zip文件
if(NOT ONNXRUNTIME_LIB)
    message(STATUS "onnxruntime.lib not found, looking for onnxruntime.zip...")
    
    # 定义可能的zip文件路径
    set(ZIP_PATHS
        "${ONNXRUNTIME_ROOT}/lib/onnxruntime.zip"
        "${ONNXRUNTIME_ROOT}/lib/mac_arm64/onnxruntime.zip"
        "${ONNXRUNTIME_ROOT}/lib/linux_x64/onnxruntime.zip"
        "${ONNXRUNTIME_ROOT}/lib/windows_x64/onnxruntime.zip"
        "${ONNXRUNTIME_ROOT}/lib/mac_x64/onnxruntime.zip"
        "${ONNXRUNTIME_ROOT}/lib/linux_aarch64/onnxruntime.zip"
    )
    
    # 检查每个可能的zip路径
    foreach(ZIP_PATH ${ZIP_PATHS})
        if(EXISTS "${ZIP_PATH}")
            message(STATUS "Found onnxruntime.zip at: ${ZIP_PATH}")
            
            # 创建解压目录
            set(EXTRACT_DIR "${CMAKE_BINARY_DIR}/onnxruntime_extracted")
            file(MAKE_DIRECTORY ${EXTRACT_DIR})
            
            # 解压zip文件
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xzf "${ZIP_PATH}"
                WORKING_DIRECTORY ${EXTRACT_DIR}
                RESULT_VARIABLE UNZIP_RESULT
            )
            
            if(UNZIP_RESULT EQUAL 0)
                message(STATUS "Successfully extracted onnxruntime.zip to ${EXTRACT_DIR}")
                
                # 重新查找解压后的库文件
                find_library(ONNXRUNTIME_LIB onnxruntime
                    HINTS ${EXTRACT_DIR}
                    NO_DEFAULT_PATH
                )
                
                if(ONNXRUNTIME_LIB)
                    message(STATUS "Using extracted onnxruntime library: ${ONNXRUNTIME_LIB}")
                    break()
                else()
                    message(WARNING "Could not find onnxruntime library in extracted directory")
                    
                    # 尝试查找其他可能的名称
                    find_library(ONNXRUNTIME_LIB libonnxruntime
                        HINTS ${EXTRACT_DIR}
                        NO_DEFAULT_PATH
                    )
                    
                    if(ONNXRUNTIME_LIB)
                        message(STATUS "Using extracted libonnxruntime library: ${ONNXRUNTIME_LIB}")
                        break()
                    endif()
                endif()
            else()
                message(WARNING "Failed to extract onnxruntime.zip, error code: ${UNZIP_RESULT}")
            endif()
        endif()
    endforeach()
endif()

if (NOT (ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIB))
    message(FATAL_ERROR "ONNX Runtime not found. Set ONNXRUNTIME_ROOT, e.g. cmake -DONNXRUNTIME_ROOT=/path/to/onnxruntime ..")
endif()
message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIB}")

# Internal implementation, linked into libkokoro and the G2P demo.
add_library(kokoro_core STATIC
    Kokoro.cpp
    Tokenizer.cpp
    ZHG2P.cpp
    PinyinFinder.cpp
    ToneSandhi.cpp
    ZHFrontend.cpp
    NeuralG2P.cpp
)
target_include_directories(kokoro_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppjieba/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppjieba/deps/limonp/include
    ${ONNXRUNTIME_INCLUDE_DIR}
)

# Public shared library: C API from include/kokoro/kokoro.h.
add_library(kokoro SHARED kokoro_c.cpp)
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

if (MSVC)
    # Static onnxruntime.lib ships without PDBs (LNK4099) and needs these system libraries.
    target_link_options(kokoro PRIVATE "/IGNORE:4099")
    target_link_libraries(kokoro PRIVATE Advapi32 Ws2_32 Shlwapi userenv Synchronization bufferoverflowu)
elseif (APPLE)
    target_link_libraries(kokoro PRIVATE "-framework Foundation" "-framework CoreFoundation")
    set_target_properties(kokoro PROPERTIES INSTALL_RPATH "@loader_path")
elseif (UNIX)
    set_target_properties(kokoro PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

# Demo Executables
add_executable(zhg2p_demo main.cpp)
target_link_libraries(zhg2p_demo PRIVATE kokoro_core)

add_executable(kokoro_demo kokoro_main.cpp)
target_link_libraries(kokoro_demo PRIVATE kokoro)

# Install: headers, libkokoro and a CMake package (find_package(kokoro) -> kokoro::kokoro).
install(TARGETS kokoro EXPORT kokoroTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
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

