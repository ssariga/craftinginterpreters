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
        vm.c
        scanner.c
        compiler.c
        object.c
        table.c
)


add_executable(test_chunk
        test_chunk.c
        chunk.c
        memory.c
        value.c
        debug.c
        vm.c
)


add_executable(benchmark_vm
        benchmark_vm.c
        chunk.c
        memory.c
        value.c
        debug.c
        vm.c
)

add_executable(test_scanner
        scanner_test.c
        scanner.c
)


add_executable(interpolation_test
        interpolation_test.c
        scanner.c
)

add_executable(conditional_test
        conditional_test.c
        compiler.c
        scanner.c
        chunk.c
        memory.c
        value.c
        debug.c
)

add_executable(dynamic_test
        main.c
        vm.c
        compiler.c
        scanner.c
        chunk.c
        memory.c
        value.c
        object.c
        table.c
        debug.c
)
target_compile_definitions(benchmark_vm PRIVATE VM_BENCHMARK)