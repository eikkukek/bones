cmake_minimum_required(VERSION 3.19.2)

project(game VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(../engine [EXCLUDE_FROM_ALL])

add_executable(game 
	src/main.cpp
)

target_link_libraries(game engine)

target_include_directories(game 
	PUBLIC src
	PUBLIC engine/headers
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})