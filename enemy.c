#include "enemy.h"
#include "game.h"
#include "memory.h"

struct Enemy*
enemy_new(float x, float y)
{
	struct Enemy *enemy = make(struct Enemy);
	if (!enemy) {
		return NULL;
	}
	enemy->x = x;
	enemy->y = y;
	enemy->hitpoints = ENEMY_INITIAL_HITPOINTS;
	enemy->body.x = x;
	enemy->body.y = y;
	enemy->body.radius = 40;
	enemy->body.type = BODY_TYPE_ENEMY;
	enemy->body.collision_mask = BODY_TYPE_PLAYER;
	enemy->body.userdata = enemy;
	return enemy;
}

void
enemy_destroy(struct Enemy *enemy)
{
	if (enemy) {
		destroy(enemy);
	}
}