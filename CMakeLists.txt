cmake_minimum_required(VERSION 3.17)
project(h265_server)

set(CMAKE_CXX_STANDARD 14)
set(Boost_INCLUDE_DIR /usr/local/include/boost)
find_package(PkgConfig REQUIRED)
find_package(Boost 1.75.0 COMPONENTS system filesystem REQUIRED)

if(Boost_FOUND)

    message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")

    message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")

    message(STATUS "Boost_VERSION: ${Boost_VERSION}")

    include_directories(${Boost_INCLUDE_DIRS})

endif()
pkg_check_modules(ffmpeg REQUIRED IMPORTED_TARGET
        libavfilter libpostproc libswscale
        libavcodec libavformat libavutil libavdevice)

add_executable(h265_server src/main.cpp)

target_link_libraries(h265_server PRIVATE PkgConfig::ffmpeg ${Boost_LIBRARIES})