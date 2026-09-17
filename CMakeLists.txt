cmake_minimum_required(VERSION 3.20)
project(Pickle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PICKLE_CORE
    attacks.cpp
    board.cpp
    evaluate.cpp
    magics.cpp
    mate_probe.cpp
    movegen.cpp
    opening_book.cpp
    search.cpp
    time_manager.cpp
    tt.cpp
    uci.cpp
    zobrist.cpp
)

function(pickle_enable_release_optimizations target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /O2)
    else()
        target_compile_options(${target} PRIVATE -O3)
    endif()
endfunction()

if(EMSCRIPTEN)
    add_executable(pickle_web ${PICKLE_CORE} wasm_api.cpp)
    set_target_properties(pickle_web PROPERTIES SUFFIX ".js")

    pickle_enable_release_optimizations(pickle_web)
    target_link_options(pickle_web PRIVATE
        -O3
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=67108864"
        "-sSTACK_SIZE=16777216"
        "-sENVIRONMENT=web,worker"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createPickleModule"
        "-sFILESYSTEM=0"
        "-sEXPORTED_FUNCTIONS=['_pickle_init','_pickle_mate_in_one','_pickle_best_move','_pickle_eval','_pickle_last_score','_pickle_last_depth','_pickle_last_nodes','_pickle_last_source','_malloc','_free']"
        "-sEXPORTED_RUNTIME_METHODS=['ccall']"
    )
else()
    add_executable(pickle ${PICKLE_CORE} datagen.cpp main.cpp)
    pickle_enable_release_optimizations(pickle)

    add_executable(pickle_regression attacks.cpp board.cpp magics.cpp movegen.cpp zobrist.cpp tests/hash_roundtrip.cpp)
    target_include_directories(pickle_regression PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    pickle_enable_release_optimizations(pickle_regression)
endif()
