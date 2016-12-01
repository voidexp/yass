#include "game.h"
#include "matlib.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Enumeration of body type bits.
 */
enum {
	BODY_TYPE_PLAYER = 1,
	BODY_TYPE_ENEMY = 1 << 1,
	BODY_TYPE_ASTEROID = 1 << 2
};

static void
add_event(struct World *world, const struct Event *evt)
{
	// extend the event queue
	if (world->event_count == world->event_queue_size) {
		size_t new_size = world->event_queue_size + EVENT_QUEUE_BASE_SIZE;
		void *new_queue = realloc(
			world->event_queue,
			sizeof(struct Event) * new_size
		);
		if (!new_queue) {
			fprintf(stderr, "event queue too long: out of memory\n");
			exit(EXIT_FAILURE);
		}
		world->event_queue = new_queue;
		world->event_queue_size = new_size;
	}

	// append the event to the end of the queue
	world->event_queue[world->event_count++] = *evt;
}

static int
handle_player_collision(struct Body *a, struct Body *b, void *userdata)
{
	if (a->type != BODY_TYPE_PLAYER) {
		return 1;
	}

	struct Event evt = { 0 };
	if (b->type == BODY_TYPE_ENEMY) {
		struct Enemy *enemy = b->userdata;
		evt.type = EVENT_ENEMY_HIT;
		evt.entity_hnd = enemy->id;
	} else if (b->type == BODY_TYPE_ASTEROID) {
		struct Asteroid *ast = b->userdata;
		evt.type = EVENT_ASTEROID_HIT;
		evt.entity_hnd = ast->id;
	}

	if (evt.type != 0) {
		struct World *world = userdata;
		add_event(world, &evt);
	}

	return 1;
}

struct World*
world_new(void)
{
	struct World *w = make(struct World);
	if (!w) {
		return NULL;
	}

	// initialize entity lists
	w->asteroid_list = list_new();
	w->projectile_list = list_new();
	if (!w->asteroid_list ||
	    !w->projectile_list) {
		world_destroy(NULL);
		return NULL;
	}

	// initialize simulation system and register collision callbacks
	w->sim = sim_new();
	if (!w->sim) {
		world_destroy(w);
		return NULL;
	}
	struct CollisionHandler handlers[] = {
		{
			handle_player_collision,
			BODY_TYPE_PLAYER | BODY_TYPE_ENEMY,
			w
		},
		{
			handle_player_collision,
			BODY_TYPE_PLAYER | BODY_TYPE_ASTEROID,
			w
		},
		{ NULL }
	};
	for (int i = 0; handlers[i].callback; i++) {
		if (!sim_add_handler(w->sim, &handlers[i])) {
			world_destroy(w);
			return NULL;
		}
	}

	// initialize event queue
	w->event_queue = malloc(sizeof(struct Event) * EVENT_QUEUE_BASE_SIZE);
	if (!w->event_queue) {
		world_destroy(w);
		return NULL;
	}
	w->event_queue_size = EVENT_QUEUE_BASE_SIZE;

	// initialize player
	w->player.hitpoints = PLAYER_INITIAL_HITPOINTS;
	w->player.y = SCREEN_HEIGHT / 2 - 50;
	w->player.speed = PLAYER_INITIAL_SPEED;
	struct Body player_body = {
		.x = w->player.x,
		.y = w->player.y,
		.radius = 40,
		.type = BODY_TYPE_PLAYER,
		.collision_mask = BODY_TYPE_ENEMY | BODY_TYPE_ASTEROID,
		.userdata = &w->player
	};
	w->player.body = player_body;

	if (!sim_add_body(w->sim, &w->player.body)) {
		world_destroy(w);
		return NULL;
	}

	return w;
}

void
world_destroy(struct World *w)
{
	if (w) {
		free(w->event_queue);
		sim_destroy(w->sim);

		// destroy entities
		struct List *lists[] = {
			w->asteroid_list,
			w->projectile_list,
			NULL,
		};
		for (unsigned i = 0; lists[i] != NULL; i++) {
			struct ListNode *node = lists[i]->head;
			while (node) {
				destroy(node->data);
				node = node->next;
			}
			list_destroy(lists[i]);
		}

		destroy(w);
	}
}

int
world_add_asteroid(struct World *world, struct Asteroid *ast)
{
	return list_add(world->asteroid_list, ast);
}

int
world_add_enemy(struct World *world, const struct Enemy *enemy)
{
	if (world->enemy_count < MAX_ENEMIES) {
		int i = world->enemy_count++;
		world->enemies[i] = *enemy;
		world->enemies[i].hitpoints = ENEMY_INITIAL_HITPOINTS;
		world->enemies[i].id = i;

		struct Body body = {
			.x = enemy->x,
			.y = enemy->y,
			.radius = 40,
			.type = BODY_TYPE_ENEMY,
			.collision_mask = BODY_TYPE_PLAYER,
			.userdata = &world->enemies[i]
		};
		world->enemies[i].body = body;
		if (!sim_add_body(world->sim, &world->enemies[i].body)) {
			return -1;
		}

		return i;
	}
	return -1;
}

int
world_add_projectile(struct World *w, struct Projectile *projectile)
{
	return list_add(w->projectile_list, projectile);
}

int
world_update(struct World *world, float dt)
{
	struct Player *plr = &world->player;

	// update physics
	static float sim_acc = 0;
	sim_acc += dt;
	while (sim_acc >= SIMULATION_STEP) {
		sim_step(world->sim, SIMULATION_STEP);
		sim_acc -= SIMULATION_STEP;
	}

	// process events
	for (size_t i = 0; i < world->event_count; i++) {
		struct Event *evt = &world->event_queue[i];
		switch (evt->type) {
		case EVENT_ENEMY_HIT:
			printf("player hit by enemy!\n");
			plr->hitpoints -= ENEMY_COLLISION_DAMAGE;
			world->enemies[evt->entity_hnd].hitpoints = 0;
			sim_remove_body(
				world->sim,
				&world->enemies[evt->entity_hnd].body
			);
			break;
		case EVENT_ASTEROID_HIT:
			printf("player hit by asteroid!\n");
			break;
		}
	}
	world->event_count = 0;

	// update player position
	float distance = dt * plr->speed;
	if (plr->actions & ACTION_MOVE_LEFT) {
		plr->x -= distance;
	} else if (plr->actions & ACTION_MOVE_RIGHT) {
		plr->x += distance;
	}

	// update player body position
	plr->body.x = plr->x;

	// handle shooting
	plr->shoot_cooldown -= dt;
	if (plr->actions & ACTION_SHOOT &&
	    plr->shoot_cooldown <= 0) {
		// reset cooldown timer
		plr->shoot_cooldown = 1.0 / PLAYER_ACTION_SHOOT_RATE;

		// shoot a projectile
		struct Projectile *prj = make(struct Projectile);
		if (!prj) {
			// TODO: set error
			return 0;
		}
		prj->x = plr->x;
		prj->y = plr->y;
		prj->xvel = 0;
		prj->yvel = -PLAYER_PROJECTILE_INITIAL_SPEED;
		prj->ttl = PLAYER_PROJECTILE_TTL;

		// TODO: handle the error
		world_add_projectile(world, prj);
	}

	// update enemies
	for (int i = 0; i < world->enemy_count; i++) {
		struct Enemy *enemy = &world->enemies[i];
		if (enemy->hitpoints <= 0) {
			continue;
		}

		// compute direction to target (player)
		Vec target = vec(plr->x, plr->y, 0, 0);
		Vec pos = vec(enemy->x, enemy->y, 0, 0);
		Vec dir;
		vec_sub(&target, &pos, &dir);
		vec_norm(&dir);

		// compute desired velocity vector
		Vec vel;
		vec_mulf(&dir, enemy->speed, &vel);

		// compute steering vector
		Vec steer, curr_vel = vec(enemy->xvel, enemy->yvel, 0, 0);
		vec_sub(&vel, &curr_vel, &steer);
		vec_clamp(&steer, 0.5);

		// compute new velocity vector
		vec_add(&curr_vel, &steer, &vel);
		vec_clamp(&vel, enemy->speed);
		enemy->xvel = vel.data[0];
		enemy->yvel = vel.data[1];

		// rotate the enemy to match direction
		enemy->rot = M_PI / 2 - atan2(vel.data[1], vel.data[0]);

		// update position
		enemy->x += vel.data[0] * dt;
		enemy->y += vel.data[1] * dt;
		enemy->body.x = enemy->x;
		enemy->body.y = enemy->y;
	}

	// update asteroids
	struct ListNode *ast_node = world->asteroid_list->head;
	while (ast_node) {
		struct Asteroid *ast = ast_node->data;
		ast->x += ast->xvel * dt;
		ast->y += ast->yvel * dt;
		ast->rot += ast->rot_speed * dt;
		if (ast->rot >= M_PI * 2) {
			ast->rot -= M_PI * 2;
		}
		ast_node = ast_node->next;
	}

	// update projectiles
	struct ListNode *prj_node = world->projectile_list->head;
	while (prj_node) {
		struct Projectile *prj = prj_node->data;
		if (prj->ttl > 0) {
			prj->x += prj->xvel * dt;
			prj->y += prj->yvel * dt;
			prj->ttl -= dt;
		}
		prj_node = prj_node->next;
	}

	return 1;
}