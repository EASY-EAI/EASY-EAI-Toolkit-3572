include(${CMAKE_CURRENT_LIST_DIR}/../../env.cmake)
##在/usr/share/cmake-3.18/Modules/目录下的Find*.cmake，都能通过这种方式被找到
##具体的变量(如${OpenCV_INCLUDE_DIRS}、${OpenCV_LIBS})会被定义在里面的相对应的Find*.cmake文件中(通常就写在开头的描述里)
#find_package(OpenCV REQUIRED)
#
set(OpenCV_INCLUDE_DIRS
    ${CMAKE_SYSROOT}/usr/include/ 
    ${CMAKE_SYSROOT}/usr/include/opencv4/ 
)
set(OpenCV_LIBS_DIRS
    ${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/lapack
    ${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/blas
)
set(OpenCV_LIBS
    opencv_core 
    opencv_imgproc 
    opencv_imgcodecs
#    opencv_calib3d 
#    opencv_dnn 
#    opencv_features2d 
#    opencv_flann 
#    opencv_highgui 
#    opencv_ml 
#    opencv_objdetect 
#    opencv_photo 
#    opencv_stitching
#    opencv_videoio 
#    opencv_video  
)

# static Library paths
# 优先使用当前系统版本对应的静态库，缺失时回退到 debian13
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/${CMAKE_BOARDSYS}/libperson_detect.a)
    set(PERSON_DETECT_LIBS_DIRS
        ${CMAKE_CURRENT_LIST_DIR}/${CMAKE_BOARDSYS}
        ${OpenCV_LIBS_DIRS}
        )
else()
    set(PERSON_DETECT_LIBS_DIRS
        ${CMAKE_CURRENT_LIST_DIR}/debian13
        ${OpenCV_LIBS_DIRS}
        )
endif()

# headfile path
set(PERSON_DETECT_INCLUDE_DIRS
    ${OpenCV_INCLUDE_DIRS} 
    ${CMAKE_CURRENT_LIST_DIR} 
    )

# c/c++ flags
set(PERSON_DETECT_LIBS 
    person_detect
    rknn3_api
    ${OpenCV_LIBS} 
    pthread 
    stdc++ 
    )
