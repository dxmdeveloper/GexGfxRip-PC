cmake_minimum_required(VERSION 3.11)
project(GexGfxRip_PC)
project(GexGfxRip_PC_TEST)

set(CMAKE_C_STANDARD 99)

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})

include_directories(src/core)
include_directories(src/app)

# core files
set(CORE_FILES
        src/core/essentials/paged_map.c
        src/core/essentials/paged_map.h
        src/core/essentials/vector.c
        src/core/essentials/vector.h
        src/core/filescanning/filescanning.c
        src/core/filescanning/filescanning.h
        src/core/filescanning/obj_gfx_and_bg.h
        src/core/filescanning/tiles.c
        src/core/filescanning/tiles.h
        src/core/graphics/gfx.c
        src/core/graphics/gfx.h
        src/core/graphics/write_png.c
        src/core/graphics/write_png.h
        src/core/helpers/basicdefs.h
        src/core/helpers/binary_parse.h
        src/core/filescanning/obj_gfx_and_bg.c
        src/core/helpers/xpgetopt/xpgetopt.c
        src/core/helpers/xpgetopt/xpgetopt.h
        src/core/filescanning/obj_gfx_and_bg.c
        src/core/essentials/bitflag_array.h
        src/core/essentials/bitflag_array.c
        src/core/filescanning/prv_filescanning.h
)

add_executable(GexGfxRip_PC
        src/app/main.c
        ${CORE_FILES}
)

add_executable(GexGfxRip_PC_TEST
        tests/combine_graphic_with_bmps_test.h
        ${CORE_FILES}
        tests/main.c
)
target_link_libraries(GexGfxRip_PC ${PNG_LIBRARY})
target_link_libraries(GexGfxRip_PC_TEST ${PNG_LIBRARY})