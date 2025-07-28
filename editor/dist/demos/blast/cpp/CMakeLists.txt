cmake_minimum_required(VERSION 3.2)

project(Blast)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    # build WASM game engine
    add_executable(BlastEngine ${ENGINE_SRC} blast.cpp)

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/BlastEngine.js"        DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/BlastEngine.wasm"      DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/BlastEngine.worker.js" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin")
    install(FILES "${ENGINE_VERSION_FILE}"                            DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin" RENAME "BlastEngine.version.txt")
else()
    # build native game engine
    add_library(BlastEngine SHARED
            ${ENGINE_SRC}/engine.cpp
            ${ENGINE_SRC}/library/library.cpp
            blast.cpp)
    # hide symbols on linux
    if (UNIX)
        target_link_options(BlastEngine PRIVATE "-Wl,--version-script=${ENGINE_SRC}/library/library.txt")
    endif()
    install(TARGETS BlastEngine DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin")
endif()

target_include_directories(BlastEngine PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(BlastEngine PRIVATE "${ENGINE_INC}")
target_compile_definitions(BlastEngine PRIVATE "${ENGINE_DEF}")
target_compile_options(BlastEngine PRIVATE ${ENGINE_COMPILE_OPTIONS})
target_link_options(BlastEngine PRIVATE ${ENGINE_LINK_OPTIONS})
target_link_libraries(BlastEngine ${ENGINE_LIBS})

