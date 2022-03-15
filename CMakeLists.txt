cmake_minimum_required(VERSION 3.21)
project(ffmpeg1 C)

set(CMAKE_C_STANDARD 11)

set(INC_DIR /usr/local/ffmpeg/include /usr/local/Cellar/sdl2/2.0.16/include)
#set(INC_DIR2 /usr/local/Cellar/sdl2/2.0.16/include)

#设置链接库路径x
set(LINK_DIR /usr/local/ffmpeg/lib  )
#set(LINK_DIR2 /usr/local/lib/libSDL2-2.0.0.dylib )

#引入头文件
include_directories(${INC_DIR})
#include_directories(${INC_DIR2})

#引入库文件
link_directories(${LINK_DIR})
#link_directories(${LINK_DIR2})
file(GLOB ffmpeg_dylib ${LINK_DIR}/*.dylib)
#file(GLOB SDL_dylib ${LINK_DIR}/libSDL2-2.0.0.dylib)


#add_executable(ffmpeg1 main.c)
#
#target_link_libraries(ffmpeg1 libavformat.dylib libavutil.dylib libswscale.dylib libavcodec.dylib)
file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
foreach(file ${files})
    get_filename_component(name ${file} NAME)
    add_executable(${name} ${file})
#    target_link_libraries(${name}  libavformat.dylib libavutil.dylib libswscale.dylib libavcodec.dylib)
    target_link_libraries(${name}  ${ffmpeg_dylib} /usr/local/lib/libSDL2-2.0.0.dylib)
endforeach()