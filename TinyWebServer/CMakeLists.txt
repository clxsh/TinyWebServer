cmake_minimum_required(VERSION 2.8)

aux_source_directory(. DIR_SRCS)

include_directories(${PROJECT_SOURCE_DIR}/base)

add_executable(tinywebserver $(DIR_SRCS))
target_link_libraries(tinywebserver server_base)

add_subdirectory(base)
# add_subdirectory(tests)