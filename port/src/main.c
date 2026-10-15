#include "game.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

Assets assets;

static RenderTexture2D screen;
static double accumulator;

// Picks where to draw the 240x160 game image in the window.
// Each game pixel covers a whole number of physical screen pixels, so pixel
// art stays sharp on any display density. Leftover space becomes black bars.
static Rectangle pixel_perfect_rect(void)
{
#if defined(PLATFORM_WEB)
    Vector2 dpi = {1, 1}; // the browser handles density, see web/shell.html
#else
    Vector2 dpi = GetWindowScaleDPI(); // 2 on a Retina display, 1 on most others
#endif
    int physicalW = GetRenderWidth();
    int physicalH = GetRenderHeight();
    int scale = physicalW / FLX_WIDTH < physicalH / FLX_HEIGHT ? physicalW / FLX_WIDTH : physicalH / FLX_HEIGHT;
    if (scale < 1) scale = 1;

    // raylib takes drawing coordinates in logical points, not physical pixels.
    float w = (float)(FLX_WIDTH * scale) / dpi.x;
    float h = (float)(FLX_HEIGHT * scale) / dpi.y;
    return (Rectangle){(GetScreenWidth() - w) / 2, (GetScreenHeight() - h) / 2, w, h};
}

// Runs the game at a fixed 60 updates per second, whatever the display rate,
// like Flixel 2.5. Slow motion shortens each update's time, not their rate.
static void frame(void)
{
    flx_input_poll();

    accumulator += GetFrameTime();
    if (accumulator > 0.25) accumulator = 0.25; // avoid a spiral after a stall
    while (accumulator >= FLX_STEP) {
        flx_elapsed = FLX_STEP * flx_time_scale;
        game_update();
        flx_input_clear();
        accumulator -= FLX_STEP;
    }

    BeginTextureMode(screen);
    ClearBackground(flx_camera.bgColor);
    game_draw();
    flx_camera_draw_fx();
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    // Render textures are stored upside down, so flip the source height.
    Rectangle source = {0, 0, FLX_WIDTH, -FLX_HEIGHT};
    Rectangle dest = pixel_perfect_rect();
    // Screen shake moves the whole picture, in game pixels.
    float pixel = dest.width / FLX_WIDTH;
    dest.x += flx_camera.shakeOffset.x * pixel;
    dest.y += flx_camera.shakeOffset.y * pixel;
    DrawTexturePro(screen.texture, source, dest, (Vector2){0, 0}, 0, WHITE);
    EndDrawing();
}

int main(void)
{
#if defined(PLATFORM_WEB)
    // On the web the canvas is exactly the game's size, one canvas pixel per
    // game pixel. The page scales it up by a whole number, see web/shell.html.
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(FLX_WIDTH, FLX_HEIGHT,
#else
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(FLX_WIDTH * FLX_ZOOM, FLX_HEIGHT * FLX_ZOOM,
#endif
               "Time Escape");
#if !defined(PLATFORM_WEB)
    ChangeDirectory(GetApplicationDirectory()); // assets sit next to the binary
    SetTargetFPS(FLX_FPS);
#endif

    assets.player = LoadTexture("assets/player.png");
    assets.enemy = LoadTexture("assets/enemy.png");
    assets.sword = LoadTexture("assets/sword.png");
    assets.claymore = LoadTexture("assets/claymore.png");
    assets.portal = LoadTexture("assets/portal.png");
    assets.sparkle = LoadTexture("assets/sparkle.png");
    assets.heart = LoadTexture("assets/heart.png");
    assets.skull = LoadTexture("assets/skull.png");
    assets.tiles = LoadTexture("assets/tiles.png");
    assets.clouds = LoadTexture("assets/clouds.png");
    assets.trees = LoadTexture("assets/trees.png");
    assets.hqMap = LoadImage("assets/hq_map.png");
    assets.levelMap = LoadImage("assets/level_map.png");
    assets.dieMap = LoadImage("assets/die_map.png");
    flx_text_load("assets/system.ttf");
    screen = LoadRenderTexture(FLX_WIDTH, FLX_HEIGHT);

    game_init();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(frame, 0, 1);
#else
    while (!WindowShouldClose()) frame();
#endif

    CloseWindow();
    return 0;
}
