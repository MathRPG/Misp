cmake_minimum_required(VERSION 3.21)
project(C_BuildYourOwnLisp C)

set(CMAKE_C_STANDARD 99)

add_executable(Main main.c mpc/mpc.c mpc/mpc.h misp/misp.c misp/misp.h)

target_link_libraries(Main -ledit -lm)
