# custom build file for Harfbuzz

cmake_minimum_required(VERSION 2.8.12)

if(POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()

message("Processing harfbuzz ...")
message(STATUS "${HARFBUZZ_FREETYPE_INCLUDE}")
message(STATUS "${HARFBUZZ_FREETYPE_LIBRARY}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set_source_files_properties(${HARFBUZZ_SRC_FILES}
     PROPERTIES
     COMPILE_FLAGS  "-utf-8 -wd4996 -wd4267 -wd4244")
endif()

set(HARFBUZZ_SRC_FILES
    src/hb-ucdn/ucdn.c
    src/hb-ft.cc
    src/hb-ucdn.cc
    src/hb-blob.cc
    src/hb-buffer.cc
    src/hb-common.cc
    src/hb-face.cc
    src/hb-font.cc
    src/hb-set.cc
    src/hb-shape.cc
    src/hb-shape-plan.cc
    src/hb-shaper.cc
    src/hb-unicode.cc
    src/hb-warning.cc
    src/hb-ot-tag.cc
    src/hb-ot-font.cc
    src/hb-ot-layout.cc
    src/hb-ot-map.cc
    src/hb-ot-shape.cc
    src/hb-ot-shape-complex-arabic.cc
    src/hb-ot-shape-complex-default.cc
    src/hb-ot-shape-complex-hangul.cc
    src/hb-ot-shape-complex-hebrew.cc
    src/hb-ot-shape-complex-indic.cc
    src/hb-ot-shape-complex-indic-table.cc
    src/hb-ot-shape-complex-myanmar.cc
    src/hb-ot-shape-complex-thai.cc
    src/hb-ot-shape-complex-tibetan.cc
    src/hb-ot-shape-complex-use.cc
    src/hb-ot-shape-complex-use-table.cc
    src/hb-ot-shape-normalize.cc
    src/hb-ot-shape-fallback.cc
    src/hb-ot-var.cc)

add_library(harfbuzz_custom_build ${HARFBUZZ_SRC_FILES})
target_link_libraries(harfbuzz_custom_build ${HARFBUZZ_FREETYPE_LIBRARY})

target_include_directories(harfbuzz_custom_build
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/src/
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hb-ucdn
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generated
    ${HARFBUZZ_FREETYPE_INCLUDE}
)

option(USE_INTEL_ATOMIC_PRIMITIVES "Platform needs to define HAVE_INTEL_ATOMIC_PRIMITIVES" FALSE)

if(USE_INTEL_ATOMIC_PRIMITIVES)
    add_definitions(-DHAVE_INTEL_ATOMIC_PRIMITIVES)
endif()

target_compile_definitions(harfbuzz_custom_build PUBLIC
    PRIVATE
    -DHAVE_OT
    -DHAVE_CONFIG_H
    -DHAVE_UCDN
    -DHB_NO_MT)

