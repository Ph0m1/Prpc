cmake_minimum_required(VERSION 3.10.0)
project(Prpc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find all dependencies here at the top level
find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)

# Set up include directories for the entire project
include_directories(
    ${CMAKE_BINARY_DIR}   
    src/include            
    ${Protobuf_INCLUDE_DIRS}
)

# Define a global variable for common libraries
# This will be visible in subdirectories like src/ and example/
set(PRPC_LIBS
    protobuf
    pthread
    zookeeper_mt
    absl::log_internal_check_op # The specific library from the error
    absl::raw_logging_internal
    absl::base
)

# Add the subdirectory. It will inherit the settings above.
add_subdirectory(src)
add_subdirectory(sample)
add_subdirectory(tests)