include(${CMAKE_CURRENT_LIST_DIR}/../../env.cmake)

# headfile path
set(LOGMANAGER_PRO_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}
    )

# static Library paths
# 优先使用当前系统版本对应的静态库，缺失时回退到 debian13
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/${CMAKE_BOARDSYS}/liblog_manager_pro.a)
    set(LOGMANAGER_PRO_LIB ${CMAKE_CURRENT_LIST_DIR}/${CMAKE_BOARDSYS}/liblog_manager_pro.a)
else()
    set(LOGMANAGER_PRO_LIB ${CMAKE_CURRENT_LIST_DIR}/debian13/liblog_manager_pro.a)
endif()

set(LOGMANAGER_PRO_LIBS
    ${LOGMANAGER_PRO_LIB}
    pthread
    )
