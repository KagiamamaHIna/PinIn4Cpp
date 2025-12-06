cmake_minimum_required(VERSION 3.12)
include(GenerateExportHeader)

project(PinIn4Cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC编码选项
if(MSVC)
    add_compile_options(/utf-8)
endif()

file(GLOB_RECURSE PinIn4CppSrc 
    CONFIGURE_DEPENDS
    "src/*.cpp" 
)

add_library(PinIn4Cpp 
    ${PinIn4CppSrc}
)

# 清理可能过时的文件
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${EXPORT_HEADER_PATH})
# 构建数据生成
set(EXPORT_HEADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/PinIn4Cpp/detail/auto/")
# 生成导出头
generate_export_header(PinIn4Cpp
    EXPORT_FILE_NAME ${EXPORT_HEADER_PATH}pinin4cpp_export.h
)

# 设置头文件搜索路径
target_include_directories(PinIn4Cpp PUBLIC
    # $<BUILD_INTERFACE:...> 只有在同一个构建系统中编译时，才使用这个路径
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# 命名空间别名
add_library(PinIn4Cpp::PinIn4Cpp ALIAS PinIn4Cpp)

# 编译示例的选项
option(PININ4CPP_BUILD_TESTS "Build test/example executable" OFF)

# 统一输出路径
set_target_properties(PinIn4Cpp PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

if(PININ4CPP_BUILD_TESTS)
    # 启用时，创建一个可执行文件而不是库
    add_executable(PinIn4Cpp_Test 
        "example/PinyinTest.cpp"
    )

    # 链接回本身
    target_link_libraries(PinIn4Cpp_Test PRIVATE PinIn4Cpp::PinIn4Cpp)

    # 可执行文件也统一
    set_target_properties(PinIn4Cpp_Test PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()