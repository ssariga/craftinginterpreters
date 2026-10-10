cmake_minimum_required(VERSION 3.16)
project(myclox C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(myclox
        main.c
        chunk.c
        memory.c
        debug.c
        value.c
)


add_executable(test_chunk
        test_chunk.c
        chunk.c
        memory.c
        value.c
        debug.c
)