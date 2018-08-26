cmake_minimum_required(VERSION 3.11)
project(fusevpk)

# compile ops
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

# find libs
find_package(PkgConfig)
pkg_check_modules(FUSE REQUIRED fuse3)

#message(STATUS "fuse include dirs are ${FUSE_INCLUDE_DIRS}")

# setup target sources and deps
add_executable(fusevpk main.cpp VPK.cpp)
target_include_directories(fusevpk PRIVATE ${FUSE_INCLUDE_DIRS})
target_link_libraries(fusevpk ${FUSE_LIBRARIES})


