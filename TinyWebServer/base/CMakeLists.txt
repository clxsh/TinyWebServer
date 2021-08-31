cmake_minimum_required (VERSION 2.8)

# SET(CMAKE_BUILD_TYPE "Debug")
# SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

aux_source_directory(. DIR_SRCS_BASE)

add_library(server_base ${DIR_SRCS_BASE})

# rt(real-time) required by time.h
target_link_libraries(server_base pthread rt)

# set_target_properties(server_base PROPERTIES OUTPUT_NAME "server_base")

add_subdirectory(tests)