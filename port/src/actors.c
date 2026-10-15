#include "game.h"


// --- Player (ProtoPlayer) --------------------------------------------------

#define PLAYER_JUMP_VELOCITY 200
#define PLAYER_JUMP_HOLD_LIMIT 0.25f // seconds the jump keeps lifting
#define PLAYER_JUMP_BURST 0.065f     // seconds of the stronger first push
#define PLAYER_HIT_PUSH 600

void player_init(Player *p, float x, float y)
{
    *p = (Player){0};
    FlxSprite *s = &p->sprite;
    flx_sprite_init(s, x, y, assets.player, 10, 10);
    s->flipWithFacing = true;
    s->acceleration.y = 2000;
    s->maxVelocity = (Vector2){120, 120};
    s->drag.x = s->maxVelocity.x * 8;

    // The hitbox is narrower and shorter than the 10x10 frame.
    s->width = 5;
    s->height -= 2;
    s->offset = (Vector2){3, 2};

    flx_sprite_add_animation(s, "idle", (int[]){1}, 1, 0, false);
    flx_sprite_add_animation(s, "idle_hasArtifact", (int[]){0}, 1, 0, false);
    flx_sprite_add_animation(s, "dead", (int[]){2}, 1, 0, false);
    flx_sprite_play(s, "idle");

    p->jump = -1; // the original starts as NaN, which behaves like -1
    p->canJump = true;
}

static void player_update_jump(Player *p)
{
    FlxSprite *s = &p->sprite;

    // A fresh press while grounded arms the jump. Holding X keeps lifting
    // for up to a quarter second, so a tap gives a short hop.
    if (p->jump == 0 && flx_key_pressed(KEY_X)) p->canJump = true;
    if (p->jump >= 0 && flx_key_down(KEY_X) && p->canJump) {
        p->jump += flx_elapsed;
        if (p->jump > PLAYER_JUMP_HOLD_LIMIT) {
            p->canJump = false;
            p->jump = -1;
        }
    } else {
        p->jump = -1;
    }

    if (p->jump > 0) {
        s->velocity.y = p->jump < PLAYER_JUMP_BURST ? -PLAYER_JUMP_VELOCITY : -s->maxVelocity.y;
    }
    if (flx_is_touching(s, FLX_DOWN)) p->jump = 0;
}

void player_update(Player *p, float mapHeight)
{
    FlxSprite *s = &p->sprite;
    flx_sprite_pre_update(s);

    if (s->alive) {
        player_update_jump(p);

        // The offset keeps the drawn body centered on the narrow hitbox.
        s->acceleration.x = 0;
        bool onScreen = flx_sprite_on_screen(s);
        if (flx_key_down(KEY_RIGHT) && onScreen) {
            s->acceleration.x = s->drag.x / 2;
            s->facing = FLX_RIGHT;
            s->offset.x = 3;
        }
        if (flx_key_down(KEY_LEFT) && onScreen) {
            s->acceleration.x = -s->drag.x / 2;
            s->facing = FLX_LEFT;
            s->offset.x = 2;
        }

        if (p->hitPushDir != 0) {
            s->velocity.y = -PLAYER_HIT_PUSH;
            s->velocity.x = p->hitPushDir * PLAYER_HIT_PUSH;
            p->hitPushDir = 0;
        }

        if (s->y > mapHeight) {
            s->alive = false;
            s->health = 0;
            p->death = DEATH_FALL;
        }

        if (flx_key_down(KEY_C) && p->weapon) claymore_shoot(p->weapon);
    }

    if (!s->alive) flx_sprite_play(s, "dead");
    else if (p->weapon) flx_sprite_play(s, "idle_hasArtifact");
    else flx_sprite_play(s, "idle");

    flx_sprite_post_update(s);
}

void player_hit_push(Player *p, int dir, float damage)
{
    FlxSprite *s = &p->sprite;
    if (!s->alive) return;

    s->health = damage >= s->health ? 0 : s->health - damage;
    if (s->health <= 0) {
        s->alive = false;
        s->health = 0;
        p->death = DEATH_HIT;
        p->throwDir = dir > 0 ? FLX_RIGHT : FLX_LEFT;
    }
    if (s->health > 1) s->health = 1;
    p->hitPushDir = dir;
}

// --- Enemy (ProtoEnemy, a cave person) ---------------------------------------

#define ENEMY_JUMP_VELOCITY 600
#define ENEMY_JUMP_TIMER_LIMIT 0.6f
#define ENEMY_HIT_TIMER_LIMIT 0.2f

void enemy_init(Enemy *e, float x, float y)
{
    *e = (Enemy){0};
    FlxSprite *s = &e->sprite;
    flx_sprite_init(s, x, y, assets.enemy, 10, 10);
    s->flipWithFacing = true;
    s->acceleration.y = 1200;
    s->maxVelocity = (Vector2){50, 120};
    s->drag.x = s->maxVelocity.x * 3;

    // Half the cave people start their idle loop on the other frame,
    // so a crowd does not bob in step.
    if (GetRandomValue(0, 1) == 1) flx_sprite_add_animation(s, "idle", (int[]){5, 0}, 2, 4, true);
    else flx_sprite_add_animation(s, "idle", (int[]){0, 5}, 2, 4, true);
    flx_sprite_add_animation(s, "enraged", (int[]){2, 3}, 2, 4, true);
    flx_sprite_add_animation(s, "dead", (int[]){4}, 1, 0, false);
    flx_sprite_add_animation(s, "hit", (int[]){0, 1}, 2, 15, true);
    flx_sprite_play(s, "idle");

    e->prevX = -46869752.0f; // any value the enemy cannot be at
    e->justHitTimer = 1e9f; // "long ago", so the first hit counts
}

void enemy_enrage(Enemy *e)
{
    if (!e->sprite.alive) return;
    flx_sprite_play(&e->sprite, "enraged");
    e->enraged = true;
}

void enemy_hit_push(Enemy *e, int dir, float damage)
{
    FlxSprite *s = &e->sprite;
    if (!s->alive || e->justHitTimer < ENEMY_HIT_TIMER_LIMIT) return;

    s->health -= damage;
    if (s->health <= 0) {
        s->alive = false;
        flx_sprite_play(s, "dead");
    }
    e->justHitTimer = 0;
    e->hitPushDir = dir;
}

// Chase the player, jump when the player is higher, and jump when stuck.
static void enemy_step_enraged_ai(Enemy *e, const Player *player)
{
    const FlxSprite *s = &e->sprite;
    e->walkingLeft = e->walkingRight = false;
    if (player->sprite.x > s->x) e->walkingLeft = true;
    else if (player->sprite.x < s->x) e->walkingRight = true;

    if (s->y - player->sprite.y > 5) e->wantsToJump = true;

    if (s->x == e->prevX) {
        e->stuckTimer += flx_elapsed;
        if (e->stuckTimer >= ENEMY_JUMP_TIMER_LIMIT) {
            e->wantsToJump = true;
            e->jumpTimer = ENEMY_JUMP_TIMER_LIMIT;
            e->stuckTimer = 0;
        }
    }
    e->prevX = s->x;
}

void enemy_update(Enemy *e, const Player *player, float mapHeight)
{
    FlxSprite *s = &e->sprite;
    if (!s->exists) return;
    flx_sprite_pre_update(s);

    if (flx_sprite_on_screen(s) && e->enraged && s->alive) {
        enemy_step_enraged_ai(e, player);
    } else {
        e->walkingLeft = e->walkingRight = e->wantsToJump = false;
    }

    if (e->wantsToJump) {
        e->jumpTimer += flx_elapsed;
        e->canJump = e->jumpTimer >= ENEMY_JUMP_TIMER_LIMIT;
        if (e->canJump) e->jumpTimer = 0;
    }
    if (e->canJump) {
        s->velocity.y = -ENEMY_JUMP_VELOCITY;
        e->canJump = false;
    }
    e->wantsToJump = false;

    // The original's names are swapped: "walkingLeft" means the player is
    // to the right, so the enemy accelerates right. The art faces left,
    // so moving right uses FLX_RIGHT to mirror it. Behavior is unchanged.
    s->acceleration.x = 0;
    if (e->walkingLeft) {
        s->acceleration.x = s->drag.x;
        s->facing = FLX_RIGHT;
    }
    if (e->walkingRight) {
        s->acceleration.x = -s->drag.x;
        s->facing = FLX_LEFT;
    }

    if (e->hitPushDir != 0) {
        s->velocity.y = -ENEMY_JUMP_VELOCITY;
        s->velocity.x = e->hitPushDir * ENEMY_JUMP_VELOCITY;
        e->hitPushDir = 0;
    }

    if (s->y > mapHeight * 2) flx_sprite_kill(s);

    e->justHitTimer += flx_elapsed;
    if (!s->alive) {
        // A dead cave person fades out over five seconds.
        flx_sprite_play(s, "dead");
        s->alpha -= flx_elapsed / 5;
        if (s->alpha <= 0.1f) flx_sprite_kill(s);
    } else if (e->justHitTimer >= ENEMY_HIT_TIMER_LIMIT) {
        e->justHitTimer = ENEMY_HIT_TIMER_LIMIT * 2;
        if (e->enraged) {
            flx_sprite_play(s, "enraged");
        } else {
            // Calm cave people turn to watch the player.
            flx_sprite_play(s, "idle");
            s->facing = s->x > player->sprite.x ? FLX_LEFT : FLX_RIGHT;
        }
    } else {
        flx_sprite_play(s, "hit");
    }

    flx_sprite_post_update(s);
}

// --- Claymore (ScottishClaymore, the sword the player carries) -------------

#define CLAYMORE_FPS 15
#define CLAYMORE_COOLDOWN 0.2f

void claymore_init(Claymore *c, Player *player)
{
    *c = (Claymore){0};
    FlxSprite *s = &c->sprite;
    flx_sprite_init(s, 0, 0, assets.claymore, 10, 15);
    s->flipWithFacing = true;
    flx_sprite_add_animation(s, "swing", (int[]){0, 1, 2, 3, 4}, 5, CLAYMORE_FPS, false);
    flx_sprite_add_animation(s, "ready", (int[]){4, 0}, 2, CLAYMORE_FPS, false);
    c->player = player;
    c->canShoot = true;
}

void claymore_shoot(Claymore *c)
{
    if (!c->canShoot) return;
    c->shooting = true;
    c->canShoot = false;
    flx_sprite_play(&c->sprite, "swing");
}

// Held beside the player, on the side the player faces.
// The sword art points the other way, so its facing is flipped.
static void claymore_place(Claymore *c)
{
    const FlxSprite *p = &c->player->sprite;
    c->sprite.facing = p->facing == FLX_LEFT ? FLX_RIGHT : FLX_LEFT;
    c->sprite.x = p->x + (p->facing == FLX_LEFT ? -7 : 8) - 3;
    c->sprite.y = p->y - 6;
}

void claymore_update(Claymore *c, Enemy *enemies, int enemyCount)
{
    FlxSprite *s = &c->sprite;
    flx_sprite_pre_update(s);
    claymore_place(c);

    if (!c->canShoot && !c->shooting) {
        c->canShootTimer += flx_elapsed;
        if (c->canShootTimer > CLAYMORE_COOLDOWN) {
            c->canShootTimer = 0;
            c->canShoot = true;
        }
    }

    // Only enemies in front of the player get hit, and pushed away.
    if (c->shooting) {
        const FlxSprite *p = &c->player->sprite;
        float playerMid = p->x + p->width / 2;
        for (int i = 0; i < enemyCount; i++) {
            Enemy *e = &enemies[i];
            if (!flx_overlaps(s, &e->sprite)) continue;
            float enemyMid = e->sprite.x + e->sprite.width / 2;
            if (enemyMid > playerMid && p->facing == FLX_RIGHT) enemy_hit_push(e, 1, 0.25f);
            else if (enemyMid <= playerMid && p->facing == FLX_LEFT) enemy_hit_push(e, -1, 0.25f);
        }
    }

    flx_sprite_post_update(s);

    // The original's animation callback: the swing ends on its last frame.
    if (flx_sprite_anim_is(s, "swing") && s->frame == 4) {
        c->canShoot = false;
        c->shooting = false;
        flx_sprite_play(s, "ready");
    }
}

// --- Artifact (the sword on its pedestal) ----------------------------------

#define ARTIFACT_BOB_HEIGHT 5
#define ARTIFACT_BOB_SPEED 0.1f // pixels per update, not per second

void artifact_init(Artifact *a, Vector2 at)
{
    *a = (Artifact){0};
    at.y -= 4;
    flx_sprite_init(&a->sprite, at.x, at.y, assets.sword, 10, 10);
    a->homeY = at.y;
    a->dir = -1;
}

// Bobs up and down between its home and 5 pixels above it.
void artifact_update(Artifact *a)
{
    FlxSprite *s = &a->sprite;
    if (!s->exists) return;
    flx_sprite_pre_update(s);
    float y = s->y + ARTIFACT_BOB_SPEED * a->dir;
    if (y <= a->homeY - ARTIFACT_BOB_HEIGHT) a->dir = 1;
    else if (y >= a->homeY) a->dir = -1;
    s->y = y;
    flx_sprite_post_update(s);
}

// --- Decorations -------------------------------------------------------------

void portal_init(FlxSprite *s, float x, float y)
{
    flx_sprite_init(s, x, y, assets.portal, 8, 8);
    flx_sprite_add_animation(s, "whoom", (int[]){0, 1, 2, 1}, 4, 10, true);
    flx_sprite_play(s, "whoom");
}

void sparkle_init(FlxSprite *s, float x, float y)
{
    flx_sprite_init(s, x, y, assets.sparkle, 10, 30);
    flx_sprite_add_animation(s, "sparkle", (int[]){0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 12, 5, true);
    flx_sprite_play(s, "sparkle");
}
