cmake_minimum_required(VERSION 3.14)
project(miniagent VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find libcurl
find_package(CURL REQUIRED)

# Threads (stream tool workers)
find_package(Threads REQUIRED)

# Sources
set(SOURCES
    src/main.cpp
    src/agent.cpp
    src/llm_client.cpp
    src/tool.cpp
    src/stream_tool.cpp
    src/stream_tools/echo.cpp
    src/stream_tools/tts.cpp
    src/tools/read_file.cpp
    src/tools/write_file.cpp
    src/tools/edit_file.cpp
    src/tools/bash.cpp
)

# Executable
add_executable(miniagent ${SOURCES})

# Include directories
target_include_directories(miniagent PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)

# Link libraries
target_link_libraries(miniagent PRIVATE CURL::libcurl Threads::Threads)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(miniagent PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Tool integration test (doesn't need curl)
set(TEST_SOURCES
    tests/test_tools.cpp
    src/tool.cpp
    src/tools/read_file.cpp
    src/tools/write_file.cpp
    src/tools/edit_file.cpp
    src/tools/bash.cpp
)
add_executable(test_tools ${TEST_SOURCES})
target_include_directories(test_tools PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(test_tools PRIVATE -Wall -Wextra -Wpedantic)
endif()
