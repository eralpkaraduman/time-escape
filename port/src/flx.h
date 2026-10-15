// flx: a small Flixel 2.5 style layer on top of raylib.
// It mirrors the parts of Flixel 2.5 that Time Escape uses: sprites with
// Flixel's motion model and touching flags, frame animations, auto-tiled
// tilemaps, Flixel's own collision separation, and a following camera with
// fade and shake. Names follow Flixel so the original code maps across.
#ifndef FLX_H
#define FLX_H

#include <stdbool.h>
#include <stddef.h>
#include "raylib.h"

// The game renders at 240x160 and shows it at 2x zoom, like the original.
#define FLX_WIDTH 240
#define FLX_HEIGHT 160
#define FLX_ZOOM 2
#define FLX_FPS 60
#define FLX_STEP (1.0f / FLX_FPS)

// Flixel treats 10000 as "no speed limit".
#define FLX_NO_LIMIT 10000.0f

// Flixel 2.5 direction flags, used for facing and for touching.
#define FLX_NONE 0x0000
#define FLX_LEFT 0x0001
#define FLX_RIGHT 0x0010
#define FLX_UP 0x0100
#define FLX_DOWN 0x1000

#define FLX_MAX_ANIMS 8
#define FLX_MAX_ANIM_FRAMES 16

// Seconds per update. Flixel scales it by timeScale for slow motion.
extern float flx_elapsed;
extern float flx_time_scale;

typedef struct {
    const char *name;
    int frames[FLX_MAX_ANIM_FRAMES];
    int frameCount;
    float fps;
    bool looped;
} FlxAnim;

typedef struct {
    float x, y, width, height;
    Vector2 last; // position before this update's move, used by collision
    Vector2 velocity, acceleration, drag, maxVelocity;
    Vector2 offset;       // drawn at (x, y) - offset, so the hitbox can be smaller
    Vector2 scrollFactor; // 0 sticks to the screen, below 1 gives parallax
    Vector2 scale;
    float angle; // degrees, clockwise, around the frame center
    float alpha;
    int facing;              // FLX_LEFT draws the graphic mirrored when flipWithFacing
    int touching;            // FLX_* sides touched by this update's collisions
    int wasTouching;
    bool immovable;
    bool exists, alive, visible;
    float health;

    Texture2D texture; // texture.id == 0 means a plain rectangle in fillColor
    Color fillColor;
    int frameWidth, frameHeight;
    int frame;
    bool flipWithFacing;

    FlxAnim anims[FLX_MAX_ANIMS];
    int animCount;
    int currentAnim; // -1 when no animation has played yet
    int animIndex;
    float animTimer;
    bool animFinished;
} FlxSprite;

typedef struct {
    int widthInTiles, heightInTiles;
    int tileSize;
    int *data; // 0 is empty, 1 and up are solid tiles
    int startingIndex; // tile value drawn with the first graphic, 1 when auto-tiled
    Texture2D tiles;
    bool visible;
} FlxTilemap;

typedef struct {
    Vector2 scroll;
    FlxSprite *target;
    Rectangle deadzone;
    Rectangle bounds;
    bool hasBounds;
    Color bgColor;

    Color fadeColor;
    float fadeAlpha, fadeDuration;
    void (*fadeComplete)(void);

    float shakeIntensity, shakeDuration;
    Vector2 shakeOffset;
} FlxCamera;

extern FlxCamera flx_camera;

// --- Sprites ---
void flx_sprite_init(FlxSprite *s, float x, float y, Texture2D texture, int frameWidth, int frameHeight);
void flx_sprite_make_graphic(FlxSprite *s, float x, float y, int width, int height, Color color);
void flx_sprite_add_animation(FlxSprite *s, const char *name, const int *frames, int frameCount, float fps, bool looped);
void flx_sprite_play(FlxSprite *s, const char *name);
bool flx_sprite_anim_is(const FlxSprite *s, const char *name);
void flx_sprite_pre_update(FlxSprite *s);  // remembers the position for collision
void flx_sprite_post_update(FlxSprite *s); // moves, animates and clears touching
void flx_sprite_draw(const FlxSprite *s);
void flx_sprite_kill(FlxSprite *s);
bool flx_sprite_on_screen(const FlxSprite *s);
Vector2 flx_sprite_screen_xy(const FlxSprite *s);
bool flx_is_touching(const FlxSprite *s, int direction);

// FlxG.overlap for two sprites. Uses the swept box of each, like Flixel.
bool flx_overlaps(const FlxSprite *a, const FlxSprite *b);
// FlxG.collide for two sprites, and for a sprite against a tilemap.
bool flx_collide(FlxSprite *a, FlxSprite *b);
bool flx_tilemap_collide(const FlxTilemap *map, FlxSprite *s);

// --- Tilemaps and PNG level layouts ---
// Black pixels become solid tiles. With autoTile, each tile picks its
// graphic from its neighbors, like FlxTilemap.AUTO.
void flx_tilemap_from_image(FlxTilemap *map, Image layout, Texture2D tiles, bool autoTile);
void flx_tilemap_unload(FlxTilemap *map);
void flx_tilemap_draw(const FlxTilemap *map);
float flx_tilemap_width(const FlxTilemap *map);
float flx_tilemap_height(const FlxTilemap *map);

// Finds every pixel of one RGB color in a layout image, in reading order,
// and returns its top-left position in world pixels.
int flx_image_points(Image layout, int rgb, int tileSize, Vector2 *out, int maxPoints);

// --- Camera ---
void flx_camera_reset(Color bgColor);
void flx_camera_follow_platformer(FlxSprite *target);
void flx_camera_set_bounds(float width, float height);
void flx_camera_update(void);
void flx_camera_shake(float intensity, float duration);
void flx_camera_fade(Color color, float duration, void (*onComplete)(void));
void flx_camera_draw_fx(void);

// --- Text ---
// Draws text in world space, wrapped to width, like an 8 pt FlxText.
void flx_text_load(const char *fontPath);
void flx_text_draw(float x, float y, float width, const char *text, Color color);

// --- Input ---
// Key presses and releases are latched per rendered frame, then consumed by
// the next fixed 60 Hz update. A press is never lost or seen twice, even on
// 120 Hz screens. Held keys use raylib's IsKeyDown directly.
void flx_input_poll(void);
void flx_input_clear(void);
bool flx_key_pressed(int key);
bool flx_key_released(int key);

float flx_random(void); // 0 <= n < 1, like Math.random()

#endif
