cmake_minimum_required(VERSION 3.14)
project(KokoroCPP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用静态链接库，但添加所有必要的链接选项

if (MSVC)
    # 基本编译选项
    add_compile_options("/utf-8")
    add_compile_options("/EHsc")
    
    # 使用静态链接的多线程运行时库
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    
    # 禁用弃用警告
    add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
    
    # 添加额外的编译器选项来解决链接问题
    add_compile_options("/Zc:inline")
    add_compile_options("/permissive-")
    add_compile_options("/Ob2")
    
    # 链接器选项在全局范围内设置
    add_link_options("/FORCE:MULTIPLE")
    
    # 添加必要的宏定义
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif()

# ONNX Runtime Configuration
# Priority 1: Check third_party/onnxruntime
set(LOCAL_ONNX_PATH "${CMAKE_SOURCE_DIR}/third_party/onnxruntime")
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

if (ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIB)
    message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIB}")
    include_directories(${ONNXRUNTIME_INCLUDE_DIR})
else()
    message(WARNING "ONNX Runtime not found. Please set ONNXRUNTIME_ROOT to your installation path.")
    message(WARNING "Example: cmake -DONNXRUNTIME_ROOT=/path/to/onnxruntime ..")
endif()

# Include paths
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/third_party/cppjieba/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party/cppjieba/deps/limonp/include)

# Shared Sources
set(LIB_SOURCES
    Kokoro.cpp
    Tokenizer.cpp
    ZHG2P.cpp
    PinyinFinder.cpp
    ToneSandhi.cpp
    ZHFrontend.cpp
    NeuralG2P.cpp
    missing_symbols.cpp
)

add_library(kokoro_core STATIC ${LIB_SOURCES})

# 所有编译选项已在全局设置中配置

# Demo Executables
add_executable(zhg2p_demo main.cpp)
target_link_libraries(zhg2p_demo kokoro_core)

# 为zhg2p_demo添加链接器选项
if (MSVC)
    target_link_options(zhg2p_demo PRIVATE "/IGNORE:4099" "/FORCE:MULTIPLE")
    
    # 添加必要的Windows系统库
    target_link_libraries(zhg2p_demo "Advapi32.lib")
    target_link_libraries(zhg2p_demo "Ws2_32.lib")
    target_link_libraries(zhg2p_demo "Shlwapi.lib")
    target_link_libraries(zhg2p_demo "userenv.lib")
    
    # 添加标准库支持
    target_link_libraries(zhg2p_demo "Synchronization.lib")
    
    # 添加安全检查支持
    target_link_libraries(zhg2p_demo "bufferoverflowu.lib")
endif()

add_executable(kokoro_demo kokoro_main.cpp)
target_link_libraries(kokoro_demo kokoro_core)

if (ONNXRUNTIME_LIB)
    target_link_libraries(kokoro_demo ${ONNXRUNTIME_LIB})
    
    # 添加额外的系统库
    if (APPLE)
        target_link_libraries(kokoro_demo "-framework Foundation" "-framework CoreFoundation")
    endif()
    
    # Windows特殊处理
    if (MSVC)
        # 添加必要的Windows系统库
        target_link_libraries(kokoro_demo "Advapi32.lib")
        target_link_libraries(kokoro_demo "Ws2_32.lib")
        target_link_libraries(kokoro_demo "Shlwapi.lib")
        target_link_libraries(kokoro_demo "userenv.lib")
        
        # 添加标准库支持
        target_link_libraries(kokoro_demo "Synchronization.lib")
        
        # 添加安全检查支持
        target_link_libraries(kokoro_demo "bufferoverflowu.lib")
        
        # 添加链接器选项
        target_link_options(kokoro_demo PRIVATE "/IGNORE:4099" "/FORCE:MULTIPLE")
    endif()
    
    # Add rpath to find the dylib at runtime
    if (APPLE)
        set_target_properties(kokoro_demo PROPERTIES INSTALL_RPATH "@loader_path/../third_party/onnxruntime/lib/mac_arm64")
    elseif (UNIX)
        set_target_properties(kokoro_demo PROPERTIES INSTALL_RPATH "$ORIGIN/../third_party/onnxruntime/lib/linux_x64")
    endif()
endif()

