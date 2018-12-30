# Writed by yijian (eyjian@qq.com, eyjian@gmail.com)

# libmuidor.a
link_directories(${CMAKE_CURRENT_SOURCE_DIR})

# libmuidor.a
add_library(muidor STATIC muidor.cpp crc32.cpp)

# muidor_agent
add_executable(muidor_agent agent.cpp crc32.cpp)
target_link_libraries(muidor_agent libmooon.a pthread dl rt z)

if (MOOON_HAVE_MYSQL)
	# muidor_master
	add_executable(muidor_master master.cpp crc32.cpp)
	target_link_libraries(muidor_master libmooon.a libmysqlclient.a pthread dl rt)
endif ()

# muidor_stress
add_executable(muidor_stress muidor_stress.cpp)
target_link_libraries(muidor_stress libmuidor.a libmooon.a pthread dl rt)

# muidor_test
add_executable(muidor_test muidor_test.cpp)
target_link_libraries(muidor_test libmuidor.a libmooon.a pthread dl rt)

# master_cli
add_executable(master_cli master_cli.cpp)
target_link_libraries(master_cli libmuidor.a libmooon.a pthread dl rt)

# 设置依赖关系
ADD_DEPENDENCIES(muidor_stress muidor)
ADD_DEPENDENCIES(muidor_test muidor)
ADD_DEPENDENCIES(master_cli muidor)

# CMAKE_INSTALL_PREFIX
install(
        TARGETS muidor
        DESTINATION lib
       )
install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/mooon/muidor       
        DESTINATION include/mooon
        PATTERN "muidor.h"
       )
if (MOOON_HAVE_MYSQL)
    install(
            TARGETS muidor_master
            DESTINATION bin
           )
endif ()
install(
        TARGETS muidor_agent muidor_stress muidor_test
        DESTINATION bin
       )
