cmake_minimum_required(VERSION 3.12)
include(GenerateExportHeader)

project(PinIn4Cpp VERSION 1.5.3)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC编码选项
if(MSVC)
    add_compile_options(/utf-8)
# else()
#     add_compile_options(-Wall)
#     add_compile_options(-Wextra)
#     add_compile_options(-pedantic)
#     add_compile_options(-Wuninitialized)
#     add_compile_options(-Wstrict-aliasing)
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
    $<INSTALL_INTERFACE:include>                          # 安装后
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

option(PININ4CPP_INSTALL "Install headers and cmake config files" ON)

if(PININ4CPP_INSTALL)

# 安装相关
include(GNUInstallDirs)

# --- 安装目标、头文件和导出信息 ---
# 这是最核心的 install 命令
install(TARGETS PinIn4Cpp
  EXPORT PinIn4CppTargets
  # 安装动态库
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  # 安装静态/导入库
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  # 安装共享库
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 安装公开的头文件
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


#生成并安装CMake包配置文件
include(CMakePackageConfigHelpers)

install(EXPORT PinIn4CppTargets
  FILE
    PinIn4CppTargets.cmake
  NAMESPACE
    PinIn4Cpp::
  DESTINATION
    "cmake"
)

configure_package_config_file(
  "cmake/PinIn4CppConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/PinIn4CppConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PinIn4Cpp"
)

# 生成版本文件
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/PinIn4CppConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

# 安装生成的配置文件
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/PinIn4CppConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PinIn4CppConfigVersion.cmake"
  DESTINATION
    "cmake"
)

# 安装开源协议文件
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_PREFIX}
)
install(DIRECTORY licenses/ DESTINATION ${CMAKE_INSTALL_PREFIX}/licenses)

endif()