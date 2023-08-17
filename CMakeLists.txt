cmake_minimum_required(VERSION 3.26)
project(GexGfxRip_PC)

set(CMAKE_C_STANDARD 99)

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})

include_directories(src/essentials)
include_directories(src/filescanning)
include_directories(src/graphics)
include_directories(src/helpers)

add_executable(GexGfxRip_PC
        src/essentials/ptr_map.c
        src/essentials/ptr_map.h
        src/essentials/stack.c
        src/essentials/stack.h
        src/essentials/vector.c
        src/essentials/vector.h
        src/filescanning/filescanning.c
        src/filescanning/filescanning.h
        src/filescanning/filescanning_obj_gfx.c
        src/filescanning/filescanning_obj_gfx.h
        src/filescanning/filescanning_tiles.c
        src/filescanning/filescanning_tiles.h
        src/filescanning/fseeking_helper.c
        src/filescanning/fseeking_helper.h
        src/graphics/gfx.c
        src/graphics/gfx.h
        src/graphics/write_png.c
        src/graphics/write_png.h
        src/helpers/basicdefs.h
        src/helpers/binary_parse.h
        src/main.c
        )

target_link_libraries(GexGfxRip_PC ${PNG_LIBRARY})