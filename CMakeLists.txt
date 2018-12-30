# Writed by yijian (eyjian@qq.com, eyjian@gmail.com)

# 3.2
cmake_minimum_required(VERSION 2.8.11)
project(muidor)

# 让make时显示编译命令
set(CMAKE_VERBOSE_MAKEFILE ON)

# 寻找已有的libmooon
if (EXISTS /usr/local/include/mooon)
    set(NEED_COMPILE_MOOON 0)
    set(MOOON_HOME /usr/local)
    include_directories(${MOOON_HOME}/include)
    link_directories(${MOOON_HOME}/lib)
elseif (EXISTS /usr/local/mooon/include/mooon)
    set(NEED_COMPILE_MOOON 0)
    set(MOOON_HOME /usr/local/mooon)
    include_directories(${MOOON_HOME}/include)
    link_directories(${MOOON_HOME}/lib)
elseif (EXISTS /usr/local/thirdparty/mooon/include/mooon)
    set(NEED_COMPILE_MOOON 0)
    set(MOOON_HOME /usr/local/thirdparty/mooon)
    include_directories(${MOOON_HOME}/include)
    link_directories(${MOOON_HOME}/lib)
elseif (EXISTS $ENV{HOME}/mooon/include/mooon)
    set(NEED_COMPILE_MOOON 0)
    set(MOOON_HOME $ENV{HOME}/mooon)
    include_directories(${MOOON_HOME}/include)
    link_directories(${MOOON_HOME}/lib)
elseif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libmooon/src/libmooon.a)
    set(NEED_COMPILE_MOOON 0)
    set(MOOON_HOME ${CMAKE_CURRENT_SOURCE_DIR}/libmooon)
    include_directories(${MOOON_HOME}/include)
    link_directories(${MOOON_HOME}/src)
elseif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libmooon.zip)
    set(NEED_COMPILE_MOOON 1)
    set(MOOON_HOME ${CMAKE_CURRENT_SOURCE_DIR}/libmooon)
    include_directories(${MOOON_HOME}/include)
    link_directories(${MOOON_HOME}/src)
    exec_program(
        unzip
        ARGS
        libmooon.zip
    )
elseif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libmooon.tar.gz)
    set(NEED_COMPILE_MOOON 1)
    set(MOOON_HOME ${CMAKE_CURRENT_SOURCE_DIR}/libmooon)
    include_directories(${MOOON_HOME}/include)
    link_directories(${MOOON_HOME}/src)
    exec_program(
        tar
        ARGS
        xzf libmooon.tar.gz
    )
else ()
    message("${Yellow}libmooon not found${ColourReset}")
    return ()
endif ()

include(${MOOON_HOME}/CMake.common)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if (NEED_COMPILE_MOOON)
    add_subdirectory(libmooon/src)
endif ()
add_subdirectory(src)
