cmake_minimum_required(VERSION 3.18)
include(FetchContent)

project(App)

# libwebsockets 
set(LWS_WITH_MBEDTLS OFF CACHE BOOL "" FORCE)
set(LWS_WITH_SSL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    lws
    GIT_REPOSITORY https://github.com/warmcat/libwebsockets.git
    GIT_TAG        v4.3.5
)
FetchContent_MakeAvailable(lws)

# source code
file(GLOB_RECURSE SOURCES "src/*.c")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/main.c")

# libraries
add_library(app_lib STATIC ${SOURCES})
target_link_libraries(app_lib PUBLIC websockets)
target_include_directories(app_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/game
    ${CMAKE_SOURCE_DIR}/src/util
    ${CMAKE_SOURCE_DIR}/src/routes
)

# executable
add_executable(${PROJECT_NAME} src/main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE app_lib)

# test
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()