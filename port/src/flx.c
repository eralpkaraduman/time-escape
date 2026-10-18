#include "flx.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

float flx_elapsed = FLX_STEP;
float flx_time_scale = 1;
FlxCamera flx_camera;

// Flixel lets objects sink up to this many pixels past their movement
// before it refuses to separate them. It stops fast objects tunnelling.
#define OVERLAP_BIAS 4

// --- Sprites ---------------------------------------------------------------

void flx_sprite_init(FlxSprite *s, float x, float y, Texture2D texture, int frameWidth, int frameHeight)
{
    *s = (FlxSprite){0};
    s->x = s->last.x = x;
    s->y = s->last.y = y;
    s->width = frameWidth;
    s->height = frameHeight;
    s->maxVelocity = (Vector2){FLX_NO_LIMIT, FLX_NO_LIMIT};
    s->scrollFactor = (Vector2){1, 1};
    s->scale = (Vector2){1, 1};
    s->alpha = 1;
    s->facing = FLX_RIGHT;
    s->exists = s->alive = s->visible = true;
    s->health = 1;
    s->texture = texture;
    s->frameWidth = frameWidth;
    s->frameHeight = frameHeight;
    s->currentAnim = -1;
}

// FlxSprite.makeGraphic: a plain colored rectangle.
void flx_sprite_make_graphic(FlxSprite *s, float x, float y, int width, int height, Color color)
{
    flx_sprite_init(s, x, y, (Texture2D){0}, width, height);
    s->fillColor = color;
}

void flx_sprite_add_animation(FlxSprite *s, const char *name, const int *frames, int frameCount, float fps, bool looped)
{
    FlxAnim *a = &s->anims[s->animCount++];
    a->name = name;
    a->frameCount = frameCount;
    memcpy(a->frames, frames, sizeof(int) * frameCount);
    a->fps = fps;
    a->looped = looped;
}

// Like Flixel 2.5, replaying the running animation does nothing,
// unless it was a one-shot animation that already finished.
void flx_sprite_play(FlxSprite *s, const char *name)
{
    for (int i = 0; i < s->animCount; i++) {
        if (strcmp(s->anims[i].name, name) != 0) continue;
        if (s->currentAnim == i && (s->anims[i].looped || !s->animFinished)) return;
        s->currentAnim = i;
        s->animIndex = 0;
        s->animTimer = 0;
        s->animFinished = false;
        s->frame = s->anims[i].frames[0];
        return;
    }
}

bool flx_sprite_anim_is(const FlxSprite *s, const char *name)
{
    return s->currentAnim >= 0 && strcmp(s->anims[s->currentAnim].name, name) == 0;
}

void flx_sprite_pre_update(FlxSprite *s)
{
    s->last = (Vector2){s->x, s->y};
}

// FlxU.computeVelocity: acceleration wins over drag, then the speed is capped.
static float compute_velocity(float velocity, float acceleration, float drag, float max)
{
    if (acceleration != 0) {
        velocity += acceleration * flx_elapsed;
    } else if (drag != 0) {
        float d = drag * flx_elapsed;
        if (velocity - d > 0) velocity -= d;
        else if (velocity + d < 0) velocity += d;
        else velocity = 0;
    }
    if (velocity != 0 && max != FLX_NO_LIMIT) {
        if (velocity > max) velocity = max;
        else if (velocity < -max) velocity = -max;
    }
    return velocity;
}

// FlxObject.updateMotion: half the velocity change applies before the move
// and half after, which is how Flixel integrates.
static void update_motion(FlxSprite *s)
{
    float half = (compute_velocity(s->velocity.x, s->acceleration.x, s->drag.x, s->maxVelocity.x) - s->velocity.x) / 2;
    s->velocity.x += half;
    s->x += s->velocity.x * flx_elapsed;
    s->velocity.x += half;

    half = (compute_velocity(s->velocity.y, s->acceleration.y, s->drag.y, s->maxVelocity.y) - s->velocity.y) / 2;
    s->velocity.y += half;
    s->y += s->velocity.y * flx_elapsed;
    s->velocity.y += half;
}

static void update_animation(FlxSprite *s)
{
    if (s->currentAnim < 0) return;
    const FlxAnim *a = &s->anims[s->currentAnim];
    if (a->fps <= 0 || (!a->looped && s->animFinished)) return;

    s->animTimer += flx_elapsed;
    float delay = 1.0f / a->fps;
    while (s->animTimer > delay) {
        s->animTimer -= delay;
        if (s->animIndex == a->frameCount - 1) {
            if (a->looped) s->animIndex = 0;
            s->animFinished = true;
        } else {
            s->animIndex++;
        }
        s->frame = a->frames[s->animIndex];
    }
}

void flx_sprite_post_update(FlxSprite *s)
{
    update_motion(s);
    s->wasTouching = s->touching;
    s->touching = FLX_NONE;
    update_animation(s);
}

Vector2 flx_sprite_screen_xy(const FlxSprite *s)
{
    return (Vector2){
        s->x - floorf(flx_camera.scroll.x * s->scrollFactor.x),
        s->y - floorf(flx_camera.scroll.y * s->scrollFactor.y),
    };
}

void flx_sprite_draw(const FlxSprite *s)
{
    if (!s->exists || !s->visible || s->alpha <= 0) return;

    Vector2 p = flx_sprite_screen_xy(s);
    // Snap to whole pixels, as Flash's copyPixels did.
    p.x = floorf(p.x - s->offset.x);
    p.y = floorf(p.y - s->offset.y);

    if (s->texture.id == 0) {
        Color c = s->fillColor;
        c.a = (unsigned char)(c.a * s->alpha);
        DrawRectangle((int)p.x, (int)p.y, (int)s->width, (int)s->height, c);
        return;
    }

    int columns = s->texture.width / s->frameWidth;
    Rectangle source = {
        (float)(s->frame % columns) * s->frameWidth,
        (float)(s->frame / columns) * s->frameHeight,
        (float)s->frameWidth,
        (float)s->frameHeight,
    };
    // A negative source width mirrors the image in raylib.
    if (s->flipWithFacing && s->facing == FLX_LEFT) source.width = -source.width;

    float w = s->frameWidth * s->scale.x;
    float h = s->frameHeight * s->scale.y;
    Rectangle dest = {p.x + s->frameWidth / 2.0f, p.y + s->frameHeight / 2.0f, w, h};
    DrawTexturePro(s->texture, source, dest, (Vector2){w / 2, h / 2}, s->angle, Fade(WHITE, s->alpha));
}

void flx_sprite_kill(FlxSprite *s)
{
    s->alive = false;
    s->exists = false;
}

bool flx_sprite_on_screen(const FlxSprite *s)
{
    Vector2 p = flx_sprite_screen_xy(s);
    return p.x + s->width > 0 && p.x < FLX_WIDTH && p.y + s->height > 0 && p.y < FLX_HEIGHT;
}

bool flx_is_touching(const FlxSprite *s, int direction)
{
    return (s->touching & direction) != 0;
}

// --- Collision -------------------------------------------------------------

bool flx_overlaps(const FlxSprite *a, const FlxSprite *b)
{
    if (!a->exists || !b->exists) return false;
    // The box from last position to this one, like Flixel's quad tree hulls.
    float ax = fminf(a->x, a->last.x), aw = a->width + fabsf(a->x - a->last.x);
    float ay = fminf(a->y, a->last.y), ah = a->height + fabsf(a->y - a->last.y);
    float bx = fminf(b->x, b->last.x), bw = b->width + fabsf(b->x - b->last.x);
    float by = fminf(b->y, b->last.y), bh = b->height + fabsf(b->y - b->last.y);
    return ax + aw > bx && ax < bx + bw && ay + ah > by && ay < by + bh;
}

static bool rects_overlap(Rectangle a, Rectangle b)
{
    return a.x + a.width > b.x && a.x < b.x + b.width && a.y + a.height > b.y && a.y < b.y + b.height;
}

// Pushes apart two overlapping objects along one axis by the overlap.
// This is FlxObject.separateX/Y with every object at mass 1 and
// elasticity 0, which is all Time Escape uses. Under those values
// two movable objects end up sharing their average velocity, and a
// movable object hitting an immovable one stops on that axis.
static void resolve(FlxSprite *a, FlxSprite *b, float overlap, bool xAxis)
{
    float *aPos = xAxis ? &a->x : &a->y;
    float *bPos = xAxis ? &b->x : &b->y;
    float *aVel = xAxis ? &a->velocity.x : &a->velocity.y;
    float *bVel = xAxis ? &b->velocity.x : &b->velocity.y;

    if (!a->immovable && !b->immovable) {
        *aPos -= overlap / 2;
        *bPos += overlap / 2;
        float average = (*aVel + *bVel) / 2;
        *aVel = *bVel = average;
    } else if (!a->immovable) {
        *aPos -= overlap;
        *aVel = *bVel;
    } else {
        *bPos += overlap;
        *bVel = *aVel;
    }
}

// FlxObject.separateX. The movement this update decides which sides touch.
// The test box spans the move along x, at the y each object started from.
static bool separate_x(FlxSprite *a, FlxSprite *b)
{
    if (a->immovable && b->immovable) return false;

    float aDelta = a->x - a->last.x;
    float bDelta = b->x - b->last.x;
    if (aDelta == bDelta) return false;

    Rectangle aRect = {a->x - (aDelta > 0 ? aDelta : 0), a->last.y, a->width + fabsf(aDelta), a->height};
    Rectangle bRect = {b->x - (bDelta > 0 ? bDelta : 0), b->last.y, b->width + fabsf(bDelta), b->height};
    if (!rects_overlap(aRect, bRect)) return false;

    float maxOverlap = fabsf(aDelta) + fabsf(bDelta) + OVERLAP_BIAS;
    float overlap = 0;
    if (aDelta > bDelta) {
        overlap = a->x + a->width - b->x;
        if (overlap > maxOverlap) return false;
        a->touching |= FLX_RIGHT;
        b->touching |= FLX_LEFT;
    } else {
        overlap = a->x - b->width - b->x;
        if (-overlap > maxOverlap) return false;
        a->touching |= FLX_LEFT;
        b->touching |= FLX_RIGHT;
    }
    if (overlap == 0) return false;
    resolve(a, b, overlap, true);
    return true;
}

// FlxObject.separateY, the same idea along y, using the new x positions.
// Flixel also carries an object along with a moving platform here.
// Nothing in this game stands on a moving object, so that part is left out.
static bool separate_y(FlxSprite *a, FlxSprite *b)
{
    if (a->immovable && b->immovable) return false;

    float aDelta = a->y - a->last.y;
    float bDelta = b->y - b->last.y;
    if (aDelta == bDelta) return false;

    Rectangle aRect = {a->x, a->y - (aDelta > 0 ? aDelta : 0), a->width, a->height + fabsf(aDelta)};
    Rectangle bRect = {b->x, b->y - (bDelta > 0 ? bDelta : 0), b->width, b->height + fabsf(bDelta)};
    if (!rects_overlap(aRect, bRect)) return false;

    float maxOverlap = fabsf(aDelta) + fabsf(bDelta) + OVERLAP_BIAS;
    float overlap = 0;
    if (aDelta > bDelta) {
        overlap = a->y + a->height - b->y;
        if (overlap > maxOverlap) return false;
        a->touching |= FLX_DOWN;
        b->touching |= FLX_UP;
    } else {
        overlap = a->y - b->height - b->y;
        if (-overlap > maxOverlap) return false;
        a->touching |= FLX_UP;
        b->touching |= FLX_DOWN;
    }
    if (overlap == 0) return false;
    resolve(a, b, overlap, false);
    return true;
}

bool flx_collide(FlxSprite *a, FlxSprite *b)
{
    if (!a->exists || !b->exists || a == b) return false;
    bool x = separate_x(a, b);
    bool y = separate_y(a, b);
    return x || y;
}

// FlxTilemap.overlapsWithCallback: each solid tile near the sprite acts as
// a still, immovable 8x8 object. All x separation runs before any y.
static bool tilemap_separate(const FlxTilemap *map, FlxSprite *s, bool (*separate)(FlxSprite *, FlxSprite *))
{
    int ts = map->tileSize;
    int startX = (int)floorf(s->x / ts);
    int startY = (int)floorf(s->y / ts);
    int endX = startX + (int)ceilf(s->width / ts) + 1;
    int endY = startY + (int)ceilf(s->height / ts) + 1;
    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;
    if (endX > map->widthInTiles) endX = map->widthInTiles;
    if (endY > map->heightInTiles) endY = map->heightInTiles;

    bool hit = false;
    for (int row = startY; row < endY; row++) {
        for (int col = startX; col < endX; col++) {
            if (map->data[row * map->widthInTiles + col] < 1) continue;
            FlxSprite tile = {.x = (float)col * ts, .y = (float)row * ts, .width = ts, .height = ts, .immovable = true};
            tile.last = (Vector2){tile.x, tile.y};
            if (separate(s, &tile)) hit = true;
        }
    }
    return hit;
}

bool flx_tilemap_collide(const FlxTilemap *map, FlxSprite *s)
{
    if (!s->exists) return false;
    bool x = tilemap_separate(map, s, separate_x);
    bool y = tilemap_separate(map, s, separate_y);
    return x || y;
}

// --- Tilemaps ----------------------------------------------------------------

static int pixel_rgb(Image image, int x, int y)
{
    Color c = GetImageColor(image, x, y);
    return (c.r << 16) | (c.g << 8) | c.b;
}

// FlxTilemap.autoTile: a 4-bit code from the solid neighbors above, right,
// below and left. Edges of the map count as solid. Runs in place, in order,
// exactly like Flixel, because later tiles read already-coded neighbors.
static void auto_tile(FlxTilemap *map, int i)
{
    int w = map->widthInTiles;
    int total = w * map->heightInTiles;
    int *d = map->data;
    if (d[i] == 0) return;

    d[i] = 0;
    if (i - w < 0 || d[i - w] > 0) d[i] += 1;               // up
    if (i % w >= w - 1 || d[i + 1] > 0) d[i] += 2;          // right
    if (i + w >= total || d[i + w] > 0) d[i] += 4;          // down
    if (i % w <= 0 || d[i - 1] > 0) d[i] += 8;              // left
    d[i] += 1;
}

void flx_tilemap_from_image(FlxTilemap *map, Image layout, Texture2D tiles, bool autoTile)
{
    map->widthInTiles = layout.width;
    map->heightInTiles = layout.height;
    map->tileSize = tiles.height;
    map->tiles = tiles;
    map->visible = true;
    map->startingIndex = autoTile ? 1 : 0;
    map->data = calloc(layout.width * layout.height, sizeof(int));

    // PNGTileMap.parseIndexes: black pixels are solid, everything else is air.
    for (int y = 0; y < layout.height; y++) {
        for (int x = 0; x < layout.width; x++) {
            map->data[y * layout.width + x] = pixel_rgb(layout, x, y) == 0 ? 1 : 0;
        }
    }
    if (autoTile) {
        for (int i = 0; i < layout.width * layout.height; i++) auto_tile(map, i);
    }
}

void flx_tilemap_unload(FlxTilemap *map)
{
    free(map->data);
    map->data = NULL;
}

float flx_tilemap_width(const FlxTilemap *map)
{
    return (float)map->widthInTiles * map->tileSize;
}

float flx_tilemap_height(const FlxTilemap *map)
{
    return (float)map->heightInTiles * map->tileSize;
}

void flx_tilemap_draw(const FlxTilemap *map)
{
    if (!map->visible) return;
    int ts = map->tileSize;
    int columns = map->tiles.width / ts;
    float scrollX = floorf(flx_camera.scroll.x);
    float scrollY = floorf(flx_camera.scroll.y);

    // Only the tiles inside the camera view.
    int startX = (int)(scrollX / ts), endX = startX + FLX_WIDTH / ts + 2;
    int startY = (int)(scrollY / ts), endY = startY + FLX_HEIGHT / ts + 2;
    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;
    if (endX > map->widthInTiles) endX = map->widthInTiles;
    if (endY > map->heightInTiles) endY = map->heightInTiles;

    for (int ty = startY; ty < endY; ty++) {
        for (int tx = startX; tx < endX; tx++) {
            int tile = map->data[ty * map->widthInTiles + tx] - map->startingIndex;
            if (map->data[ty * map->widthInTiles + tx] < 1) continue;
            Rectangle source = {(float)(tile % columns) * ts, (float)(tile / columns) * ts, (float)ts, (float)ts};
            Vector2 at = {(float)tx * ts - scrollX, (float)ty * ts - scrollY};
            DrawTextureRec(map->tiles, source, at, WHITE);
        }
    }
}

// PNGTileMap.parsePoints with the default top-left offset.
int flx_image_points(Image layout, int rgb, int tileSize, Vector2 *out, int maxPoints)
{
    int count = 0;
    for (int y = 0; y < layout.height; y++) {
        for (int x = 0; x < layout.width; x++) {
            if (pixel_rgb(layout, x, y) != rgb || count == maxPoints) continue;
            out[count++] = (Vector2){(float)x * tileSize, (float)y * tileSize};
        }
    }
    return count;
}

// --- Camera ----------------------------------------------------------------

// A new camera per screen, like FlxG.resetCameras on a state switch.
void flx_camera_reset(Color bgColor)
{
    flx_camera = (FlxCamera){0};
    flx_camera.bgColor = bgColor;
}

// FlxCamera.STYLE_PLATFORMER: a tall, narrow box a little above center.
// The camera only scrolls when the target leaves that box.
void flx_camera_follow_platformer(FlxSprite *target)
{
    float w = FLX_WIDTH / 8.0f;
    float h = FLX_HEIGHT / 3.0f;
    flx_camera.target = target;
    flx_camera.deadzone = (Rectangle){(FLX_WIDTH - w) / 2, (FLX_HEIGHT - h) / 2 - h * 0.25f, w, h};
}

void flx_camera_set_bounds(float width, float height)
{
    flx_camera.bounds = (Rectangle){0, 0, width, height};
    flx_camera.hasBounds = true;
}

void flx_camera_update(void)
{
    FlxCamera *c = &flx_camera;
    if (c->target) {
        float tx = c->target->x, ty = c->target->y;
        float edge = tx - c->deadzone.x;
        if (c->scroll.x > edge) c->scroll.x = edge;
        edge = tx + c->target->width - c->deadzone.x - c->deadzone.width;
        if (c->scroll.x < edge) c->scroll.x = edge;
        edge = ty - c->deadzone.y;
        if (c->scroll.y > edge) c->scroll.y = edge;
        edge = ty + c->target->height - c->deadzone.y - c->deadzone.height;
        if (c->scroll.y < edge) c->scroll.y = edge;
    }
    if (c->hasBounds) {
        if (c->scroll.x < c->bounds.x) c->scroll.x = c->bounds.x;
        if (c->scroll.x > c->bounds.x + c->bounds.width - FLX_WIDTH) c->scroll.x = c->bounds.x + c->bounds.width - FLX_WIDTH;
        if (c->scroll.y < c->bounds.y) c->scroll.y = c->bounds.y;
        if (c->scroll.y > c->bounds.y + c->bounds.height - FLX_HEIGHT) c->scroll.y = c->bounds.y + c->bounds.height - FLX_HEIGHT;
    }

    if (c->fadeAlpha > 0 && c->fadeAlpha < 1) {
        c->fadeAlpha += flx_elapsed / c->fadeDuration;
        if (c->fadeAlpha >= 1) {
            c->fadeAlpha = 1;
            if (c->fadeComplete) c->fadeComplete();
        }
    }

    if (c->shakeDuration > 0) {
        c->shakeDuration -= flx_elapsed;
        if (c->shakeDuration <= 0) {
            c->shakeOffset = (Vector2){0, 0};
        } else {
            float range = c->shakeIntensity;
            c->shakeOffset.x = flx_random() * range * FLX_WIDTH * 2 - range * FLX_WIDTH;
            c->shakeOffset.y = flx_random() * range * FLX_HEIGHT * 2 - range * FLX_HEIGHT;
        }
    }
}

void flx_camera_shake(float intensity, float duration)
{
    flx_camera.shakeIntensity = intensity;
    flx_camera.shakeDuration = duration;
    flx_camera.shakeOffset = (Vector2){0, 0};
}

void flx_camera_fade(Color color, float duration, void (*onComplete)(void))
{
    if (flx_camera.fadeAlpha > 0) return; // a fade is already running
    flx_camera.fadeColor = color;
    flx_camera.fadeDuration = duration;
    flx_camera.fadeComplete = onComplete;
    flx_camera.fadeAlpha = 0.000001f; // above zero starts the fade
}

void flx_camera_draw_fx(void)
{
    if (flx_camera.fadeAlpha > 0) {
        DrawRectangle(0, 0, FLX_WIDTH, FLX_HEIGHT, Fade(flx_camera.fadeColor, flx_camera.fadeAlpha));
    }
}

// --- Text ------------------------------------------------------------------

// Flixel's "system" font is 8 px per em. raylib sizes a font by ascent plus
// descent instead, which is 10 px for this build of the font. At 10, one
// font pixel lands on exactly one screen pixel, and lines are 10 px apart.
#define FONT_SIZE 10
#define TEXT_GUTTER 2 // Flash text fields pad their text by 2 px

static Font font;

// FONT_BITMAP rasterizes each glyph pixel fully on or off, so the pixel
// font stays crisp instead of anti-aliased.
void flx_text_load(const char *fontPath)
{
    int dataSize = 0;
    unsigned char *data = LoadFileData(fontPath, &dataSize);
    font = (Font){.baseSize = FONT_SIZE, .glyphCount = 95};
    font.glyphs = LoadFontData(data, dataSize, FONT_SIZE, NULL, 0, FONT_BITMAP, &font.glyphCount);
    Image atlas = GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount, FONT_SIZE, 2, 0);
    font.texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    UnloadFileData(data);
}

static float text_width(const char *start, int length)
{
    char line[256];
    if (length > 255) length = 255;
    memcpy(line, start, length);
    line[length] = '\0';
    return MeasureTextEx(font, line, FONT_SIZE, 0).x;
}

static void draw_line(const char *start, int length, Vector2 at, Color color)
{
    char line[256];
    if (length > 255) length = 255;
    memcpy(line, start, length);
    line[length] = '\0';
    DrawTextEx(font, line, at, FONT_SIZE, 0, color);
}

// Word-wraps like a Flash TextField: explicit newlines always break, and
// a word that would pass the field width moves to the next line.
void flx_text_draw(float x, float y, float width, const char *text, Color color)
{
    float maxWidth = width - TEXT_GUTTER * 2;
    Vector2 at = {floorf(x - flx_camera.scroll.x) + TEXT_GUTTER, floorf(y - flx_camera.scroll.y) + TEXT_GUTTER};

    const char *lineStart = text;
    while (*lineStart) {
        const char *newline = strchr(lineStart, '\n');
        const char *lineEnd = newline ? newline : lineStart + strlen(lineStart);

        // Take as many whole words as fit, then draw that piece.
        const char *pieceStart = lineStart;
        while (pieceStart < lineEnd || pieceStart == lineStart) {
            const char *cut = lineEnd;
            if (text_width(pieceStart, (int)(lineEnd - pieceStart)) > maxWidth) {
                const char *lastSpace = NULL;
                for (const char *p = pieceStart; p < lineEnd; p++) {
                    if (*p == ' ' && text_width(pieceStart, (int)(p - pieceStart)) <= maxWidth) lastSpace = p;
                }
                if (lastSpace) cut = lastSpace;
            }
            draw_line(pieceStart, (int)(cut - pieceStart), at, color);
            at.y += FONT_SIZE;
            pieceStart = cut;
            while (pieceStart < lineEnd && *pieceStart == ' ') pieceStart++;
            if (pieceStart >= lineEnd) break;
        }
        if (!newline) break;
        lineStart = newline + 1;
    }
}

// --- Input -----------------------------------------------------------------

#define KEY_SLOTS 512

static bool pressed[KEY_SLOTS];
static bool released[KEY_SLOTS];
static bool touchDown[KEY_SLOTS];
static bool padDown[KEY_SLOTS];

// Touch events arrive between frames, so they latch presses and releases
// just like the keyboard does in flx_input_poll.
#if defined(__EMSCRIPTEN__)
EMSCRIPTEN_KEEPALIVE
#endif
void flx_touch_key(int key, bool down)
{
    if (key < 0 || key >= KEY_SLOTS || touchDown[key] == down) return;
    touchDown[key] = down;
    if (down) pressed[key] = true;
    else released[key] = true;
}

bool flx_key_down(int key)
{
    return IsKeyDown(key) || touchDown[key] || padDown[key];
}

// Any connected gamepad stands in for the keys too. The D-pad and left stick
// are the arrows, the bottom face button is X and the left face button is C.
static void gamepad_poll(void)
{
    static const int keys[] = {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_X, KEY_C};
    bool down[6] = {0};
    for (int g = 0; g < 4; g++) {
        if (!IsGamepadAvailable(g)) continue;
        float x = GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_X);
        float y = GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_Y);
        down[0] |= x < -0.5f || IsGamepadButtonDown(g, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        down[1] |= x > 0.5f || IsGamepadButtonDown(g, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        down[2] |= y < -0.5f || IsGamepadButtonDown(g, GAMEPAD_BUTTON_LEFT_FACE_UP);
        down[3] |= y > 0.5f || IsGamepadButtonDown(g, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        down[4] |= IsGamepadButtonDown(g, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        down[5] |= IsGamepadButtonDown(g, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
    }
    for (int i = 0; i < 6; i++) {
        if (padDown[keys[i]] == down[i]) continue;
        padDown[keys[i]] = down[i];
        if (down[i]) pressed[keys[i]] = true;
        else released[keys[i]] = true;
    }
}

void flx_input_poll(void)
{
    gamepad_poll();
    for (int key = 0; key < KEY_SLOTS; key++) {
        if (IsKeyPressed(key)) pressed[key] = true;
        if (IsKeyReleased(key)) released[key] = true;
    }
}

void flx_input_clear(void)
{
    memset(pressed, 0, sizeof pressed);
    memset(released, 0, sizeof released);
}

bool flx_key_pressed(int key)
{
    return pressed[key];
}

bool flx_key_released(int key)
{
    return released[key];
}

float flx_random(void)
{
    return (float)GetRandomValue(0, 999999) / 1000000.0f;
}
