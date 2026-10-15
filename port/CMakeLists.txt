cmake_minimum_required(VERSION 3.24)
project(timeescape C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# raylib 6.0 is downloaded on the first configure and checked against its
# SHA-256 hash. It builds as a static library, so it ends up inside the game
# binary or WebAssembly file, with no separate library to ship.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "" FORCE)
endif()
FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
    URL_HASH SHA256=2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(raylib)

add_executable(timeescape src/main.c src/game.c src/actors.c src/flx.c)
target_link_libraries(timeescape raylib)

if(EMSCRIPTEN)
    target_compile_definitions(timeescape PRIVATE PLATFORM_WEB)
    # itch.io wants index.html at the root of the uploaded zip.
    set_target_properties(timeescape PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")
    target_link_options(timeescape PRIVATE
        # The raylib wiki advises a fixed heap over memory growth.
        # The game needs only a few MB, so 32 MB leaves plenty of room.
        # emscripten_set_main_loop drives frames, so ASYNCIFY is not needed.
        -sUSE_GLFW=3
        -sINITIAL_MEMORY=33554432
        -sENVIRONMENT=web
        --preload-file ${CMAKE_SOURCE_DIR}/assets@assets
        --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html)
    # cmake --build build-web --target itch  ->  build-web/timeescape-itch.zip
    add_custom_target(itch
        COMMAND ${CMAKE_COMMAND} -E tar cf timeescape-itch.zip --format=zip
            index.html index.js index.wasm index.data
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS timeescape)
    # cmake --build build-web --target pages  ->  build-web/pages/, the GitHub
    # Pages site: a cover page at the root and the game itself under play/.
    set(PAGES_DIR "${CMAKE_BINARY_DIR}/pages")
    add_custom_target(pages
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${PAGES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PAGES_DIR}/play" "${PAGES_DIR}/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/web/site.html" "${PAGES_DIR}/index.html"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/timeescape-cover-630x500.png" "${PAGES_DIR}/cover.png"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/screenshots" "${PAGES_DIR}/screenshots"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/system.ttf" "${PAGES_DIR}/fonts/system.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy index.html index.js index.wasm index.data "${PAGES_DIR}/play"
        COMMAND ${CMAKE_COMMAND} -E touch "${PAGES_DIR}/.nojekyll"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS timeescape
        VERBATIM)
else()
    if(NOT MSVC)
        target_link_libraries(timeescape m)
    endif()
    # Copy assets next to the binary. main.c changes into that folder.
    add_custom_command(TARGET timeescape POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:timeescape>/assets)
endif()

# Logic checks, desktop only: ./build/timeescape_check from this folder.
if(NOT EMSCRIPTEN)
    add_executable(timeescape_check tests/check.c src/game.c src/actors.c src/flx.c)
    target_link_libraries(timeescape_check raylib)
    if(NOT MSVC)
        target_link_libraries(timeescape_check m)
    endif()
endif()
