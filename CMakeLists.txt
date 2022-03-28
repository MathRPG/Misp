cmake_minimum_required(VERSION 3.21)
project(C_BuildYourOwnLisp C)

set(CMAKE_C_STANDARD 99)

add_executable(C_BuildYourOwnLisp main.c)

target_link_libraries(C_BuildYourOwnLisp edit)
