cmake_minimum_required(VERSION 3.21)
project(ffmpeg1 C)

set(CMAKE_C_STANDARD 11)

set(INC_DIR /usr/local/ffmpeg/include)

#设置链接库路径
set(LINK_DIR /usr/local/ffmpeg/lib)

#引入头文件
include_directories(${INC_DIR})

#引入库文件
link_directories(${LINK_DIR})



#add_executable(ffmpeg1 main.c)
#
#target_link_libraries(ffmpeg1 libavformat.dylib libavutil.dylib libswscale.dylib libavcodec.dylib)
file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
foreach(file ${files})
    get_filename_component(name ${file} NAME)
    add_executable(${name} ${file})
    target_link_libraries(${name}  libavformat.dylib libavutil.dylib libswscale.dylib libavcodec.dylib)
endforeach()