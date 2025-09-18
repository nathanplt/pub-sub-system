cmake_minimum_required(VERSION 3.20)
project(messenger)

set(CMAKE_CXX_STANDARD 20)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
find_package(Boost REQUIRED COMPONENTS system)

# Include directories
include_directories(${ZMQ_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} src)

# Create executables
add_executable(pub_mt src/app/pub_mt.cpp src/bus/publisher.cpp src/bus/subscriber.cpp src/bus/metrics.cpp)
add_executable(sub_pool src/app/sub_pool.cpp src/bus/publisher.cpp src/bus/subscriber.cpp src/bus/metrics.cpp)

# Link libraries
target_link_libraries(pub_mt /opt/homebrew/Cellar/zeromq/4.3.5_1/lib/libzmq.dylib ${Boost_LIBRARIES} pthread)
target_link_libraries(sub_pool /opt/homebrew/Cellar/zeromq/4.3.5_1/lib/libzmq.dylib ${Boost_LIBRARIES} pthread)