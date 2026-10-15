// Time Escape (post-compo version of "Stuff From The Past", Ludum Dare 21).
// Actors and shared game state. Names follow the original ActionScript:
// ProtoPlayer is the player, ProtoEnemy a cave person, ScottishClaymore the
// sword you carry back, Artifact the sword waiting on its pedestal.
#ifndef GAME_H
#define GAME_H

#include "flx.h"

typedef struct {
    Texture2D player, enemy, sword, claymore, portal, sparkle, heart, skull;
    Texture2D tiles, clouds, trees;
    Image hqMap, levelMap, dieMap; // level layouts, one pixel per tile
} Assets;

extern Assets assets;

typedef enum { DEATH_NONE, DEATH_FALL, DEATH_HIT } DeathReason;

typedef struct Claymore Claymore;

typedef struct {
    FlxSprite sprite;
    float jump; // seconds the jump key has been held, -1 when not jumping
    bool canJump;
    int hitPushDir;
    Claymore *weapon;
    DeathReason death;
    int throwDir; // FLX_LEFT or FLX_RIGHT, the way a fatal hit throws the body
} Player;

typedef struct {
    FlxSprite sprite;
    bool enraged;
    bool walkingLeft, walkingRight; // named as in the original, see enemy_update
    bool wantsToJump, canJump;
    float jumpTimer, stuckTimer, justHitTimer;
    float prevX;
    int hitPushDir;
} Enemy;

struct Claymore {
    FlxSprite sprite;
    Player *player;
    bool shooting, canShoot;
    float canShootTimer;
};

typedef struct {
    FlxSprite sprite;
    float homeY;
    int dir;
} Artifact;

void player_init(Player *p, float x, float y);
void player_update(Player *p, float mapHeight);
void player_hit_push(Player *p, int dir, float damage);

void enemy_init(Enemy *e, float x, float y);
void enemy_update(Enemy *e, const Player *player, float mapHeight);
void enemy_hit_push(Enemy *e, int dir, float damage);
void enemy_enrage(Enemy *e);

void claymore_init(Claymore *c, Player *player);
void claymore_shoot(Claymore *c);
void claymore_update(Claymore *c, Enemy *enemies, int enemyCount);

void artifact_init(Artifact *a, Vector2 at);
void artifact_update(Artifact *a);

void portal_init(FlxSprite *s, float x, float y);
void sparkle_init(FlxSprite *s, float x, float y);

void game_init(void);
void game_update(void);
void game_draw(void);

#endif
