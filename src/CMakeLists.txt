# Writed by yijian (eyjian@qq.com, eyjian@gmail.com)

# libmooon_uidor.a
link_directories(${CMAKE_CURRENT_SOURCE_DIR})

# libmooon_uidor.a
add_library(mooon_uidor STATIC uidor.cpp crc32.cpp)

# uid_agent
add_executable(uid_agent agent.cpp crc32.cpp)
target_link_libraries(uid_agent libmooon.a pthread dl rt z)

if (MOOON_HAVE_MYSQL)
	# uid_master
	add_executable(uid_master master.cpp crc32.cpp)
	target_link_libraries(uid_master libmooon.a libmysqlclient.a pthread dl rt)
endif ()

# uid_stress
add_executable(uid_stress uid_stress.cpp)
target_link_libraries(uid_stress libmooon_uidor.a libmooon.a pthread dl rt)

# uid_test
add_executable(uid_test uid_test.cpp)
target_link_libraries(uid_test libmooon_uidor.a libmooon.a pthread dl rt)

# master_cli
add_executable(master_cli master_cli.cpp)
target_link_libraries(master_cli libmooon_uidor.a libmooon.a pthread dl rt)

# 设置依赖关系
ADD_DEPENDENCIES(uid_stress mooon_uidor)
ADD_DEPENDENCIES(uid_test mooon_uidor)
ADD_DEPENDENCIES(master_cli mooon_uidor)

# CMAKE_INSTALL_PREFIX
install(
        TARGETS mooon_uidor
        DESTINATION lib
       )
install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/mooon/uidor       
        DESTINATION include/mooon
        PATTERN "uidor.h"
       )
if (MOOON_HAVE_MYSQL)
    install(
            TARGETS uid_master
            DESTINATION bin
           )
endif ()
install(
        TARGETS uid_agent uid_stress uid_test master_cli
        DESTINATION bin
       )
