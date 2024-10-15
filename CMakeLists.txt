cmake_minimum_required(VERSION 3.10.2)
project(tcpserver)

set(CMAKE_CXX_STANDARD 17)

file(GLOB sources "src/[a-zA-Z]*.cpp")
file(GLOB_RECURSE public_headers "include/${PROJECT_NAME}/[a-zA-Z]*.h")
file(GLOB private_headers "src/[a-zA-Z]*.h")

set(library_sources
        ${sources}
        ${public_headers}
        ${private_headers}
)

add_library(${PROJECT_NAME} STATIC ${library_sources})

target_include_directories(${PROJECT_NAME}
        PUBLIC include
        PRIVATE include/${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
        fastlog
        buffer
        timeutils)