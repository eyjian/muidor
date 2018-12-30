# Writed by yijian (eyjian@qq.com, eyjian@gmail.com)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include/mooon)

# 源代码
set(
    MOOON_NET_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/data_channel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/epollable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/epoller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ip_address.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libssh2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/listener.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcp_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcp_waiter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/udp_socket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kafka_producer.cpp
    CACHE INTERNAL
    MOOON_NET_SRC
)

# libmooon_net.a
#add_library(
#    mooon_net
#    STATIC
#    ${MOOON_NET_SRC}
#)

# CMAKE_INSTALL_PREFIX
install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../include/mooon/net        
        DESTINATION include/mooon
        PATTERN "*.h"
       )
