cmake_minimum_required(VERSION 3.20)
project(messenger VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

find_package(Boost REQUIRED COMPONENTS system)

# Include directories
include_directories(${ZMQ_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Source files
set(BUS_SOURCES
    src/bus/types.hpp
    src/bus/publisher.hpp
    src/bus/publisher.cpp
    src/bus/subscriber.hpp
    src/bus/subscriber.cpp
    src/bus/metrics.hpp
    src/bus/metrics.cpp
)

set(PUB_MT_SOURCES
    src/app/pub_mt.cpp
    ${BUS_SOURCES}
)

set(SUB_POOL_SOURCES
    src/app/sub_pool.cpp
    ${BUS_SOURCES}
)

# Create executables
add_executable(pub_mt ${PUB_MT_SOURCES})
add_executable(sub_pool ${SUB_POOL_SOURCES})

# Link directories
link_directories(${ZMQ_LIBRARY_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
link_directories(/opt/homebrew/Cellar/zeromq/4.3.5_1/lib)

# Link libraries
target_link_libraries(pub_mt /opt/homebrew/Cellar/zeromq/4.3.5_1/lib/libzmq.dylib ${Boost_LIBRARIES} pthread)
target_link_libraries(sub_pool /opt/homebrew/Cellar/zeromq/4.3.5_1/lib/libzmq.dylib ${Boost_LIBRARIES} pthread)

# Compiler flags
target_compile_options(pub_mt PRIVATE ${ZMQ_CFLAGS_OTHER})
target_compile_options(sub_pool PRIVATE ${ZMQ_CFLAGS_OTHER})

# Set C++ standard
set_target_properties(pub_mt PROPERTIES CXX_STANDARD 20)
set_target_properties(sub_pool PROPERTIES CXX_STANDARD 20)

# Install targets
install(TARGETS pub_mt sub_pool DESTINATION bin)
