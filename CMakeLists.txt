cmake_minimum_required(VERSION 3.16)
project(spix C)

# spix — один заголовок, поэтому CMake нужен только для тестов, демо и бенчмарка.
option(SPIX_BUILD_TESTS "Собрать тесты корректности" ON)
option(SPIX_BUILD_BENCH "Собрать бенчмарк (не гоняется в CI)" ON)
option(SPIX_BUILD_DEMO  "Собрать демо" ON)

function(spix_target name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(${name} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
    if(NOT MSVC)
        target_compile_options(${name} PRIVATE -Wall -Wextra -O2)
    endif()
    target_link_libraries(${name} PRIVATE m)
endfunction()

if(SPIX_BUILD_TESTS)
    enable_testing()
    spix_target(test_spix tests/test_spix.c)
    add_test(NAME spix_correctness COMMAND test_spix)
endif()

if(SPIX_BUILD_BENCH)
    spix_target(bench_spix bench/bench_spix.c)
endif()

if(SPIX_BUILD_DEMO)
    spix_target(spix_demo examples/demo.c)
endif()
