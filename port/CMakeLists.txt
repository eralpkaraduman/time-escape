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
    # The web page around the game and its touch controls are shared with
    # Kurban Bayrami. They are downloaded from that repo at a pinned commit,
    # and each file is checked against its SHA-256 hash.
    set(SHARED_WEB_COMMIT 7fa49b284b8f15ba1f1b34adddabfcd59c14fb10)
    set(SHARED_WEB_URL "https://raw.githubusercontent.com/eralpkaraduman/kurban-bayrami/${SHARED_WEB_COMMIT}/port/web")
    set(WEB_DIR "${CMAKE_BINARY_DIR}/shared-web")
    foreach(entry
            "shell.html.in=89586e4c5c8e951db598b665f56df6e109c3ebc20645fef0359ae0705cfe6f8c"
            "touch-controls.js=c7d1d21acb13b73e98d4559bda5b53170a013c5e0ba38e9700a79b7d86c4cdf2"
            "touch-controls.css=46aa24d82ea792a5ddb62fab4f286ff360b0916f45f5d5d3cc81fcc7a4e19462")
        string(REPLACE "=" ";" entry "${entry}")
        list(GET entry 0 name)
        list(GET entry 1 hash)
        file(DOWNLOAD "${SHARED_WEB_URL}/${name}" "${WEB_DIR}/${name}" EXPECTED_HASH SHA256=${hash})
    endforeach()

    # Fill in the page for this game. Key codes are raylib's: arrows 262-265,
    # X 88, C 67. Up and down only matter as "any key" on the death screen,
    # so the D-pad is left and right only.
    set(WEB_TITLE "Time Escape: Stuff From The Past")
    set(WEB_TOUCH_DPAD "{ left: 263, right: 262 }")
    set(WEB_TOUCH_BUTTONS "[{ label: 'X', caption: 'Jump', key: 88 }, { label: 'C', caption: 'Sword', key: 67 }]")
    configure_file("${WEB_DIR}/shell.html.in" "${CMAKE_BINARY_DIR}/shell.html" @ONLY)

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
        --shell-file ${CMAKE_BINARY_DIR}/shell.html)
    # Relink when the page changes, and put the touch controls beside it.
    set_target_properties(timeescape PROPERTIES LINK_DEPENDS "${CMAKE_BINARY_DIR}/shell.html")
    add_custom_command(TARGET timeescape POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${WEB_DIR}/touch-controls.js" "${WEB_DIR}/touch-controls.css"
            "${CMAKE_BINARY_DIR}"
        VERBATIM)
    set(WEB_FILES index.html index.js index.wasm index.data touch-controls.js touch-controls.css)
    # cmake --build build-web --target itch  ->  build-web/timeescape-itch.zip
    add_custom_target(itch
        COMMAND ${CMAKE_COMMAND} -E tar cf timeescape-itch.zip --format=zip ${WEB_FILES}
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
        COMMAND ${CMAKE_COMMAND} -E copy ${WEB_FILES} "${PAGES_DIR}/play"
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
