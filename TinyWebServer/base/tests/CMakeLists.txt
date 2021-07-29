
# SET(CMAKE_BUILD_TYPE "Debug")
# SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

add_executable(LoggerTest LoggerTest.cc)
target_link_libraries(LoggerTest server_base)