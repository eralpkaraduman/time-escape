// Logic checks that need no window. Run from the port folder:
//   ./build/timeescape_check
#include "../src/game.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

Assets assets; // textures stay empty; only update logic runs here

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    Image hq = LoadImage("assets/hq_map.png");
    Image level = LoadImage("assets/level_map.png");
    assert(hq.data && level.data);

    // Auto-tiling keeps every black pixel solid and picks one of 16 graphics.
    Texture2D tiles = {.width = 128, .height = 8};
    FlxTilemap map = {0};
    flx_tilemap_from_image(&map, hq, tiles, true);
    assert(map.tileSize == 8 && map.widthInTiles == 30 && map.heightInTiles == 30);
    int solid = 0;
    for (int i = 0; i < 30 * 30; i++) {
        assert(map.data[i] >= 0 && map.data[i] <= 16);
        if (map.data[i] > 0) solid++;
    }
    assert(solid == 219); // black pixels in hq_map.png

    // Level layout markers parse from their pixel colors.
    Vector2 points[64];
    assert(flx_image_points(hq, 0x0094FF, 8, points, 64) == 5);  // portals
    assert(flx_image_points(level, 0xFF0000, 8, points, 64) == 26); // cave people
    assert(flx_image_points(level, 0x0000FF, 8, points, 64) == 1);  // spawn

    // The player falls from the HQ spawn point, lands, and stays on the floor.
    Vector2 spawn;
    flx_image_points(hq, 0x0000FF, 8, &spawn, 1);
    Player player;
    player_init(&player, spawn.x, spawn.y);
    bool landed = false;
    for (int i = 0; i < 180; i++) {
        flx_tilemap_collide(&map, &player.sprite);
        if (flx_is_touching(&player.sprite, FLX_DOWN)) landed = true;
        player_update(&player, flx_tilemap_height(&map));
    }
    assert(landed);
    float restY = player.sprite.y;
    for (int i = 0; i < 60; i++) {
        flx_tilemap_collide(&map, &player.sprite);
        player_update(&player, flx_tilemap_height(&map));
    }
    assert(fabsf(player.sprite.y - restY) < 1);
    assert(player.sprite.alive && player.death == DEATH_NONE);

    // Ten hits from the front kill the player and pick a throw direction.
    for (int i = 0; i < 10; i++) player_hit_push(&player, -1, 0.1f);
    assert(!player.sprite.alive && player.death == DEATH_HIT && player.throwDir == FLX_LEFT);

    // Two cave people walking into each other stop at the contact point.
    Enemy a, b;
    enemy_init(&a, 0, 0);
    enemy_init(&b, 12, 0);
    a.sprite.last = (Vector2){0, 0};
    b.sprite.last = (Vector2){12, 0};
    a.sprite.x = 3;
    b.sprite.x = 9;
    a.sprite.velocity.x = 50;
    b.sprite.velocity.x = -50;
    assert(flx_collide(&a.sprite, &b.sprite));
    assert(fabsf((a.sprite.x + a.sprite.width) - b.sprite.x) < 0.001f);
    assert(a.sprite.velocity.x == 0 && b.sprite.velocity.x == 0);
    assert(flx_is_touching(&a.sprite, FLX_RIGHT) && flx_is_touching(&b.sprite, FLX_LEFT));

    // A cave person dies after four sword hits, with a pause between hits.
    Enemy target;
    enemy_init(&target, 0, 0);
    for (int i = 0; i < 4; i++) {
        target.justHitTimer = 1;
        enemy_hit_push(&target, 1, 0.25f);
    }
    assert(!target.sprite.alive);

    flx_tilemap_unload(&map);
    UnloadImage(hq);
    UnloadImage(level);
    puts("all checks passed");
    return 0;
}
