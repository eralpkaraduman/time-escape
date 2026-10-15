// The five screens of the game. Each original FlxState is one enter_/update_/
// draw_ group here. Only one screen exists at a time, so they share a struct.
#include "game.h"

#include <math.h>
#include <string.h>

#define MAX_ENEMIES 64
#define MAX_PORTALS 8
#define BLOOD_PARTICLES 25

// Colors from the original, as 0xAARRGGBB in the ActionScript.
static const Color SKY_COLOR = {0x8B, 0xD0, 0xFC, 0xFF};
static const Color DIE_BG_COLOR = {0xA6, 0x9A, 0x90, 0xFF};
static const Color DIRT_WALL_COLOR = {0x7C, 0x58, 0x1E, 0xFF};
static const Color LIGHT_RAY_COLOR = {0xFF, 0xF8, 0xAF, 0xFF};
static const Color HEALTH_BG_COLOR = {0xFF, 0x00, 0x00, 0x22};
static const Color BLOOD_COLOR = {0xFF, 0x00, 0x00, 0xFF};

// Layout pixel colors, as 0xRRGGBB.
#define KEY_SPAWN 0x0000FF
#define KEY_PORTAL 0x0094FF
#define KEY_DIRT_WALL 0xFF6A00
#define KEY_LIGHT_RAY 0xFFD800
#define KEY_ARTIFACT 0xDC00FF
#define KEY_ENEMY 0xFF0000

typedef enum { STATE_NONE, STATE_INTRO, STATE_HQ, STATE_LEVEL, STATE_DIE, STATE_WIN } StateId;

// The health bar and heart in the top-left corner of a level (UI.as).
typedef struct {
    float drawP;      // shown health, eases toward the real value
    float beatTimer;
    bool panic;       // once health drops under half, the heart blinks for good
    bool heartVisible;
} Hud;

// The blood fountain on the death screen (FlxEmitter with FlxParticles).
typedef struct {
    bool on;
    float x, y;
    float minSpeedX, maxSpeedX;
    float timer;
    int next; // particles are reused in turn, like FlxGroup.recycle
    FlxSprite particles[BLOOD_PARTICLES];
} Blood;

static struct {
    StateId id;
    FlxTilemap map;
    Player player;

    // HQState and ProtoState
    FlxSprite portals[MAX_PORTALS];
    FlxSprite sparkles[MAX_PORTALS];
    int portalCount;
    Vector2 spawnPoint;
    float rePortalTimer;

    // HQState
    bool gameWon;
    bool canFade;
    float gameWonTimer;

    // ProtoState
    Enemy enemies[MAX_ENEMIES];
    int enemyCount;
    Artifact artifact;
    Claymore claymore;
    FlxSprite dirtWall, lightRay;
    bool canHitPlayer;
    float enemyCanHitTimer;
    Hud hud;

    // ScreenDieHit
    FlxSprite skull;
    bool textVisible;
    bool fall;
    bool firstHitDone;
    Vector2 force;
    Blood blood;

    // IntroState
    int textsIndex;
} state;

// FlxG.switchState waits for the next update, so the rest of the current
// update still runs. The pending screen and its arguments wait here.
static struct {
    StateId id;
    Vector2 playerScreenXY;
    int throwTo;
    int facing;
    bool fall;
} pending;

// Survives screen changes, like the original's static variables.
static int lastEnteredPortal = -1;            // HQState.lastEnteredTo
static bool broughtBackArtifact = false;      // ProtoState.broughtBackArtifactThroughPortal
static bool gotArtifact = false;              // ProtoState.gotArtifact

static void switch_state(StateId id)
{
    pending.id = id;
}

static void unload_state(void)
{
    flx_tilemap_unload(&state.map);
    memset(&state, 0, sizeof state);
}

// --- IntroState ----------------------------------------------------------

static const char *INTRO_TEXTS[] = {
    "LUDUM DARE 20 - ESCAPE COMPO GAME\n"
    "'STUFF FROM THE PAST' by Eralp Karaduman (AUKYR)\n"
    "Twitter: @aukyr\n"
    "\n"
    "Press x to skip\n",

    "This game is about %60 complete, so let me tell you what it is about:\n\n"
    "This dude you are playing is responsible for adjustments made in time\n\n"
    "In a mission he had forgot claymore swords in the prehistoric era.\n"
    "Balance of time was distrubed, and to fix this, he must find and remove the swords.\n",

    "To travel back and forth in time, he uses portals\n"
    "Pass through 5 portals, and find the swords\n\n"
    "(Every portal opents to the same place for now\n"
    "I will make 4 more levels tomorrow for the jam hopefuly)\n",

    "As you get the sword, prehistoric people will be angry\n"
    "They will attack you\n"
    "Because they believe that the sword is a gift from the gods\n"
    "And they don't want you to take it back\n\n"
    "you must ESCAPE back to the portal\n\n"
    "If you can get back and don't die\n"
    "YOU WIN\n",

    "GOOD LUCK!\n"
    "It's sad I could not finish this game for the compo\n"
    "This is my first real attempt on compo, worked hard on it\n"
    "So I'm submitting anyway... ENJOY\n\n"
    "Now, I must sleep.. work tomorrow\n",

    "Oh, I almost forgot..\n\n"
    "Arrow keys : MOVE\n"
    "X : JUMP\n"
    "C : ATTACK (When you have the sword)\n",

    ": )\n",
};
#define INTRO_TEXT_COUNT (int)(sizeof INTRO_TEXTS / sizeof INTRO_TEXTS[0])

static void enter_intro(void)
{
    flx_camera_reset(BLACK);
    state.textsIndex = 0;
}

static void update_intro(void)
{
    if (!flx_key_released(KEY_X)) return;
    state.textsIndex++;
    if (state.textsIndex >= INTRO_TEXT_COUNT) switch_state(STATE_HQ);
}

static void draw_intro(void)
{
    if (state.textsIndex < INTRO_TEXT_COUNT) flx_text_draw(0, 0, FLX_WIDTH, INTRO_TEXTS[state.textsIndex], WHITE);
}

// --- WinState --------------------------------------------------------------

static void enter_win(void)
{
    flx_camera_reset(WHITE);
}

static void update_win(void)
{
    if (flx_key_released(KEY_X)) switch_state(STATE_INTRO);
}

static void draw_win(void)
{
    flx_text_draw(0, 0, FLX_WIDTH, "!! YOU WON !!\n\nPress X to go again!\n", BLACK);
}

// --- HQState: the hub with five portals ------------------------------------

static void on_win_fade_complete(void)
{
    switch_state(STATE_WIN);
}

static void enter_hq(void)
{
    flx_camera_reset(SKY_COLOR);
    flx_time_scale = 1;

    flx_tilemap_from_image(&state.map, assets.hqMap, assets.tiles, true);
    int ts = state.map.tileSize;
    flx_image_points(assets.hqMap, KEY_SPAWN, ts, &state.spawnPoint, 1);

    Vector2 points[MAX_PORTALS];
    state.portalCount = flx_image_points(assets.hqMap, KEY_PORTAL, ts, points, MAX_PORTALS);
    for (int i = 0; i < state.portalCount; i++) {
        portal_init(&state.portals[i], points[i].x, points[i].y);
        sparkle_init(&state.sparkles[i], points[i].x - 2, points[i].y - 20);
    }

    // Coming back through a portal flings the player out toward the middle.
    Vector2 fling = {0, 0};
    if (lastEnteredPortal != -1) {
        state.spawnPoint = points[lastEnteredPortal];
        fling.x = state.spawnPoint.x > flx_tilemap_width(&state.map) / 2 ? -1500 : 1500;
        fling.y = -1500;
    }
    player_init(&state.player, state.spawnPoint.x, state.spawnPoint.y);
    state.player.sprite.velocity = fling;
    state.rePortalTimer = 0;

    flx_camera_follow_platformer(&state.player.sprite);
    flx_camera_set_bounds(flx_tilemap_width(&state.map), flx_tilemap_height(&state.map));

    state.canFade = true;
    if (broughtBackArtifact) {
        state.gameWon = true;
        broughtBackArtifact = false;
    }
}

static void update_hq(void)
{
    if (state.gameWon) {
        state.gameWonTimer += flx_elapsed;
        if (state.gameWonTimer > 2 && state.canFade) {
            flx_camera_fade(WHITE, 1.5f, on_win_fade_complete);
            state.canFade = false;
        }
    }

    FlxSprite *ps = &state.player.sprite;
    flx_tilemap_collide(&state.map, ps);
    for (int i = 0; i < state.portalCount; i++) {
        if (state.gameWon || state.rePortalTimer < 1) break;
        if (flx_overlaps(ps, &state.portals[i])) {
            lastEnteredPortal = i;
            switch_state(STATE_LEVEL); // every portal leads to the same level
            break;
        }
    }
    if (ps->y > flx_tilemap_height(&state.map)) {
        ps->x = state.spawnPoint.x;
        ps->y = state.spawnPoint.y;
    }
    state.rePortalTimer += flx_elapsed;

    for (int i = 0; i < state.portalCount; i++) {
        flx_sprite_pre_update(&state.sparkles[i]);
        flx_sprite_post_update(&state.sparkles[i]);
        flx_sprite_pre_update(&state.portals[i]);
        flx_sprite_post_update(&state.portals[i]);
    }
    player_update(&state.player, flx_tilemap_height(&state.map));
}

static void draw_hq(void)
{
    flx_tilemap_draw(&state.map);
    for (int i = 0; i < state.portalCount; i++) flx_sprite_draw(&state.sparkles[i]);
    for (int i = 0; i < state.portalCount; i++) flx_sprite_draw(&state.portals[i]);
    flx_sprite_draw(&state.player.sprite);
}

// --- ProtoState: the prehistoric level --------------------------------------

static void enter_level(void)
{
    flx_camera_reset(SKY_COLOR);
    gotArtifact = false;

    flx_tilemap_from_image(&state.map, assets.levelMap, assets.tiles, true);
    int ts = state.map.tileSize;
    float mapW = flx_tilemap_width(&state.map), mapH = flx_tilemap_height(&state.map);

    // A brown wall behind the cave and a beam of light over the sword.
    Vector2 p;
    flx_image_points(assets.levelMap, KEY_DIRT_WALL, ts, &p, 1);
    flx_sprite_make_graphic(&state.dirtWall, p.x + 8, 0, (int)mapW, (int)mapH * 2, DIRT_WALL_COLOR);
    flx_image_points(assets.levelMap, KEY_LIGHT_RAY, ts, &p, 1);
    flx_sprite_make_graphic(&state.lightRay, p.x + 1, 0, 8, (int)mapH * 2, LIGHT_RAY_COLOR);

    flx_image_points(assets.levelMap, KEY_SPAWN, ts, &state.spawnPoint, 1);
    Vector2 artifactPoint;
    flx_image_points(assets.levelMap, KEY_ARTIFACT, ts, &artifactPoint, 1);

    state.portalCount = 1;
    portal_init(&state.portals[0], state.spawnPoint.x, state.spawnPoint.y);

    Vector2 enemyPoints[MAX_ENEMIES];
    state.enemyCount = flx_image_points(assets.levelMap, KEY_ENEMY, ts, enemyPoints, MAX_ENEMIES);
    for (int i = 0; i < state.enemyCount; i++) enemy_init(&state.enemies[i], enemyPoints[i].x, enemyPoints[i].y);

    // The player pops out of the portal, flung up and to the right.
    player_init(&state.player, state.spawnPoint.x, state.spawnPoint.y);
    state.player.sprite.velocity = (Vector2){1500, -1500};
    state.rePortalTimer = 0;

    sparkle_init(&state.sparkles[0], artifactPoint.x, artifactPoint.y - 20);
    artifact_init(&state.artifact, artifactPoint);

    flx_camera_set_bounds(mapW, mapH);
    flx_camera_follow_platformer(&state.player.sprite);

    state.canHitPlayer = true;
    state.hud = (Hud){.drawP = 1, .heartVisible = true};
}

static void take_artifact(void)
{
    for (int i = 0; i < state.enemyCount; i++) enemy_enrage(&state.enemies[i]);
    flx_sprite_kill(&state.artifact.sprite);
    if (gotArtifact) return;

    gotArtifact = true;
    claymore_init(&state.claymore, &state.player);
    state.player.weapon = &state.claymore;
    flx_camera_shake(0.03f, 0.3f);
}

// An angry cave person hurts the player only from the front,
// and knocks the player back away from it.
static void enemy_touches_player(Enemy *e)
{
    if (!state.canHitPlayer || !e->enraged || !e->sprite.alive) return;
    const FlxSprite *ps = &state.player.sprite;
    float enemyMid = e->sprite.x + e->sprite.width / 2;
    float playerMid = ps->x + ps->width / 2;
    if (enemyMid > playerMid && ps->facing == FLX_RIGHT) player_hit_push(&state.player, -1, 0.1f);
    else if (enemyMid <= playerMid && ps->facing == FLX_LEFT) player_hit_push(&state.player, 1, 0.1f);
    state.canHitPlayer = false;
}

static void update_hud(void)
{
    Hud *h = &state.hud;
    h->drawP += (state.player.sprite.health - h->drawP) / 15;
    if (h->drawP < 0) h->drawP = 0;
    if (h->drawP > 1) h->drawP = 1;
    if (h->drawP < 0.5f) h->panic = true;

    h->beatTimer += flx_elapsed;
    if (!h->panic) {
        h->heartVisible = true;
    } else if (h->beatTimer >= 0.2f) {
        h->heartVisible = !h->heartVisible;
        h->beatTimer = 0;
    }
}

static void update_level(void)
{
    FlxSprite *ps = &state.player.sprite;
    float mapH = flx_tilemap_height(&state.map);

    for (int i = 0; i < state.enemyCount; i++) {
        for (int j = i + 1; j < state.enemyCount; j++) flx_collide(&state.enemies[i].sprite, &state.enemies[j].sprite);
    }
    flx_tilemap_collide(&state.map, ps);
    for (int i = 0; i < state.enemyCount; i++) flx_tilemap_collide(&state.map, &state.enemies[i].sprite);

    for (int i = 0; i < state.enemyCount; i++) {
        if (flx_overlaps(ps, &state.enemies[i].sprite)) enemy_touches_player(&state.enemies[i]);
    }
    if (flx_overlaps(ps, &state.artifact.sprite)) take_artifact();
    if (flx_overlaps(ps, &state.portals[0]) && state.rePortalTimer >= 1) {
        if (gotArtifact) broughtBackArtifact = true;
        switch_state(STATE_HQ);
    }

    state.rePortalTimer += flx_elapsed;
    if (!state.canHitPlayer) {
        state.enemyCanHitTimer += flx_elapsed;
        if (state.enemyCanHitTimer > 0.5f) {
            state.canHitPlayer = true;
            state.enemyCanHitTimer = 0;
        }
    }

    flx_sprite_pre_update(&state.portals[0]);
    flx_sprite_post_update(&state.portals[0]);
    for (int i = 0; i < state.enemyCount; i++) enemy_update(&state.enemies[i], &state.player, mapH);
    player_update(&state.player, mapH);
    flx_sprite_pre_update(&state.sparkles[0]);
    flx_sprite_post_update(&state.sparkles[0]);
    artifact_update(&state.artifact);
    update_hud();
    if (state.player.weapon) claymore_update(&state.claymore, state.enemies, state.enemyCount);

    // ProtoState.playerIsDead: show where the player was on screen.
    if (state.player.death != DEATH_NONE && pending.id == STATE_NONE) {
        pending.playerScreenXY = flx_sprite_screen_xy(ps);
        pending.fall = state.player.death == DEATH_FALL;
        pending.throwTo = state.player.throwDir;
        pending.facing = ps->facing;
        switch_state(STATE_DIE);
    }
}

// Two copies of a 240 px wide layer, repeated as the camera scrolls.
static void draw_parallax(Texture2D layer, float scrollFactor)
{
    float offset = fmodf(floorf(flx_camera.scroll.x * scrollFactor), (float)layer.width);
    float y = -floorf(flx_camera.scroll.y);
    DrawTexture(layer, (int)-offset, (int)y, WHITE);
    DrawTexture(layer, (int)(layer.width - offset), (int)y, WHITE);
}

static void draw_hud(void)
{
    // Fixed to the screen, so it ignores the camera.
    const int margin = 3;
    int barX = margin + 7, barY = margin + 1;
    int barW = FLX_WIDTH - margin * 2 - 7;
    DrawRectangle(barX, barY, barW, 2, HEALTH_BG_COLOR);
    if (state.player.sprite.alive) DrawRectangle(barX, barY, (int)(barW * state.hud.drawP), 2, BLOOD_COLOR);
    if (state.hud.heartVisible) DrawTexture(assets.heart, 3, 3, WHITE);
}

static void draw_level(void)
{
    draw_parallax(assets.trees, 0.7f);
    draw_parallax(assets.clouds, 0.4f);
    flx_sprite_draw(&state.dirtWall);
    flx_sprite_draw(&state.lightRay);
    flx_tilemap_draw(&state.map);
    flx_sprite_draw(&state.portals[0]);
    for (int i = 0; i < state.enemyCount; i++) flx_sprite_draw(&state.enemies[i].sprite);
    flx_sprite_draw(&state.player.sprite);
    flx_sprite_draw(&state.sparkles[0]);
    flx_sprite_draw(&state.artifact.sprite);
    draw_hud();
    if (state.player.weapon) flx_sprite_draw(&state.claymore.sprite);
}

// --- ScreenDieHit: the body tumbles down a shaft in slow motion -------------

#define DIE_TIME_SCALE 0.3f
#define DIE_FORCE 150
#define BLOOD_SPEED 50
#define BLOOD_FREQUENCY 0.008f // seconds between drops
#define BLOOD_GRAVITY 300

static void blood_start(void)
{
    Blood *b = &state.blood;
    b->on = true;
    b->timer = 0;
    // Blood sprays away from the way the body faces.
    if (state.player.sprite.facing == FLX_LEFT) { b->minSpeedX = 0; b->maxSpeedX = BLOOD_SPEED; }
    else { b->minSpeedX = -BLOOD_SPEED; b->maxSpeedX = 0; }
    for (int i = 0; i < BLOOD_PARTICLES; i++) {
        flx_sprite_make_graphic(&b->particles[i], 0, 0, 1, 1, BLOOD_COLOR);
        b->particles[i].exists = false;
    }
}

static void blood_update(void)
{
    Blood *b = &state.blood;
    if (!b->on) return;
    b->timer += flx_elapsed;
    while (b->timer > BLOOD_FREQUENCY) {
        b->timer -= BLOOD_FREQUENCY;
        FlxSprite *drop = &b->particles[b->next];
        b->next = (b->next + 1) % BLOOD_PARTICLES;
        drop->exists = drop->alive = true;
        drop->x = drop->last.x = b->x;
        drop->y = drop->last.y = b->y;
        drop->velocity.x = b->minSpeedX + flx_random() * (b->maxSpeedX - b->minSpeedX);
        drop->velocity.y = -BLOOD_SPEED + flx_random() * BLOOD_SPEED;
        drop->acceleration.y = BLOOD_GRAVITY;
    }
    for (int i = 0; i < BLOOD_PARTICLES; i++) {
        if (!b->particles[i].exists) continue;
        flx_sprite_pre_update(&b->particles[i]);
        flx_sprite_post_update(&b->particles[i]);
    }
}

static void enter_die(void)
{
    flx_camera_reset(DIE_BG_COLOR);
    state.fall = pending.fall;
    state.force = (Vector2){DIE_FORCE, DIE_FORCE};

    // An invisible shaft the body bounces down. Only its walls matter.
    flx_tilemap_from_image(&state.map, assets.dieMap, assets.tiles, false);
    state.map.visible = false;
    float mapH = flx_tilemap_height(&state.map);
    float bottom = mapH - FLX_HEIGHT; // top of the last screen of the shaft

    if (!state.fall) flx_time_scale = DIE_TIME_SCALE;

    // The body appears where the player was on screen, at the shaft bottom.
    // After a fall it drops in from the top of that screen instead.
    Vector2 at = pending.playerScreenXY;
    if (state.fall) at.y = 0;
    at.y += bottom;

    flx_sprite_init(&state.skull, 105, bottom + 65, assets.skull, assets.skull.width, assets.skull.height);
    state.skull.scale = (Vector2){3, 3};
    state.skull.alpha = 0;

    player_init(&state.player, at.x, at.y);
    FlxSprite *ps = &state.player.sprite;
    ps->maxVelocity.y = 300;
    ps->alive = false;
    if (state.fall) {
        ps->facing = pending.facing;
    } else {
        ps->maxVelocity.x = 300;
        ps->velocity.x = pending.throwTo == FLX_LEFT ? -state.force.x : state.force.x;
        ps->velocity.y = -state.force.y;
        ps->facing = pending.throwTo;
    }

    flx_camera_set_bounds(flx_tilemap_width(&state.map), mapH);
    flx_camera_follow_platformer(ps);
    if (!state.fall) blood_start();
}

// After a fall, the slow motion and blood start at the first impact.
static void die_first_hit(void)
{
    if (state.firstHitDone) return;
    state.firstHitDone = true;
    if (state.fall) {
        flx_time_scale = DIE_TIME_SCALE;
        blood_start();
    }
}

static void update_die(void)
{
    FlxSprite *ps = &state.player.sprite;
    bool anyKey = IsKeyDown(KEY_X) || IsKeyDown(KEY_C) || IsKeyDown(KEY_UP) ||
                  IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_DOWN);
    if (state.skull.alpha > 0.5f && anyKey) switch_state(STATE_HQ);

    if (state.skull.alpha < 1) state.skull.alpha = fminf(1, state.skull.alpha + flx_elapsed / 2);
    if (state.skull.alpha > 0.4f) state.textVisible = true;

    state.blood.x = ps->x + (ps->facing == FLX_RIGHT ? 7 : 3);
    state.blood.y = ps->y + 6;

    // Each bounce off a wall, floor or ceiling halves the force.
    flx_tilemap_collide(&state.map, ps);
    if (state.force.y > 0.2f) {
        if (flx_is_touching(ps, FLX_LEFT)) {
            ps->velocity.x = state.force.x;
            ps->facing = FLX_RIGHT;
            state.force.x /= 2;
        }
        if (flx_is_touching(ps, FLX_RIGHT)) {
            ps->velocity.x = -state.force.x;
            ps->facing = FLX_LEFT;
            state.force.x /= 2;
            die_first_hit();
        }
        if (flx_is_touching(ps, FLX_DOWN)) {
            ps->velocity.y = -state.force.y;
            ps->velocity.x = ps->facing == FLX_LEFT ? -state.force.x : state.force.x;
            state.force.x /= 2;
            state.force.y /= 2;
            die_first_hit();
        }
        if (flx_is_touching(ps, FLX_UP)) {
            ps->velocity.y = state.force.y;
            ps->velocity.x = ps->facing == FLX_LEFT ? -state.force.x : state.force.x;
            state.force.x /= 2;
            state.force.y /= 2;
            die_first_hit();
        }
    }

    player_update(&state.player, flx_tilemap_height(&state.map));
    blood_update();
}

static void draw_die(void)
{
    if (state.textVisible) {
        flx_text_draw(-3, flx_tilemap_height(&state.map) - FLX_HEIGHT, 300,
                      "     YOU DIED IN A TIME WHICH WAS NOT YOURS\n"
                      "YOUR CORPSE WILL ROT BEFORE YOU WILL BE BORN\n"
                      "                          X TO TRY AGAIN",
                      WHITE);
    }
    flx_sprite_draw(&state.skull);
    flx_sprite_draw(&state.player.sprite);
    for (int i = 0; i < BLOOD_PARTICLES; i++) flx_sprite_draw(&state.blood.particles[i]);
}

// --- Entry points ---------------------------------------------------------

static void enter(StateId id)
{
    unload_state();
    state.id = id;
    switch (id) {
    case STATE_INTRO: enter_intro(); break;
    case STATE_HQ: enter_hq(); break;
    case STATE_LEVEL: enter_level(); break;
    case STATE_DIE: enter_die(); break;
    case STATE_WIN: enter_win(); break;
    case STATE_NONE: break;
    }
}

void game_init(void)
{
    switch_state(STATE_INTRO);
}

void game_update(void)
{
    if (pending.id != STATE_NONE) {
        StateId next = pending.id;
        pending.id = STATE_NONE;
        enter(next);
    }

    switch (state.id) {
    case STATE_INTRO: update_intro(); break;
    case STATE_HQ: update_hq(); break;
    case STATE_LEVEL: update_level(); break;
    case STATE_DIE: update_die(); break;
    case STATE_WIN: update_win(); break;
    case STATE_NONE: break;
    }
    flx_camera_update();
}

void game_draw(void)
{
    switch (state.id) {
    case STATE_INTRO: draw_intro(); break;
    case STATE_HQ: draw_hq(); break;
    case STATE_LEVEL: draw_level(); break;
    case STATE_DIE: draw_die(); break;
    case STATE_WIN: draw_win(); break;
    case STATE_NONE: break;
    }
}
