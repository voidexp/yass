#include "font.h"
#include "game.h"
#include "image.h"
#include "layout.h"
#include "renderer.h"
#include "state.h"
#include "text.h"
#include "texture.h"

struct Button {
	int (*on_click)(void);
	int x, y;
	int w, h;
};

/*** UI STATE ***/
static struct {
	int show_upgrades_win;
} ui_state = {
	0
};

/*** RESOURCES ***/
static struct Font *font_dbg = NULL;
static struct Font *font_hud = NULL;

static struct Text *text_fps = NULL;
static struct Text *text_render_time = NULL;
static struct Text *text_credits = NULL;

static struct Image *hp_bar = NULL;
static struct Image *hp_bar_bg = NULL;
static struct Image *upgrades_win = NULL;
static struct Image *upgrades_weapon_frame = NULL;

static struct Texture *tex_hp_bar_green = NULL;
static struct Texture *tex_hp_bar_bg = NULL;
static struct Texture *tex_win = NULL;
static struct Texture *tex_frame = NULL;

// TEXTURES
static const struct TextureRes {
	const char *file;
	struct Texture **var;
} textures[] = {
	{ "data/art/UI/squareGreen.png", &tex_hp_bar_green },
	{ "data/art/UI/squareRed.png", &tex_hp_bar_bg },
	{ "data/art/UI/metalPanel_red.png", &tex_win },
	{ "data/art/UI/metalPanel_plate.png", &tex_frame },
	{ NULL }
};

// FONTS
static const struct {
	const char *file;
	unsigned size;
	struct Font **var;
} fonts[] = {
	{ "data/fonts/courier.ttf", 16, &font_dbg },
	{ "data/fonts/kenvector_future_thin.ttf", 16, &font_hud },
	{ NULL }
};

// LAYOUT ELEMENTS
static struct Element *e_root = NULL;
static struct Element *e_text_credits = NULL;
static struct Element *e_hp_bar = NULL;
static struct Element *e_text_fps = NULL;
static struct Element *e_text_render_time = NULL;
static struct Element *e_upgrades_win = NULL;
static struct Element *e_upgrades_weapons_frame = NULL;

static const struct {
	struct Element **var, **parent;
	struct Anchors anchors;
	struct Margins margins;
	unsigned width, height;
	int x, y;
} layout[] = {
	// root element (screen)
	{
		.var = &e_root,
		.parent = NULL,
		.width = SCREEN_WIDTH,
		.height = SCREEN_HEIGHT
	},
	// hitpoints bar
	{
		.var = &e_hp_bar,
		.parent = &e_root,
		.anchors = {
			.left = ANCHOR_LEFT,
			.top = ANCHOR_TOP
		},
		.margins = {
			.left = 20,
			.top = 20
		},
		.width = 200,
		.height = 26
	},
	// credits text image
	{
		.var = &e_text_credits,
		.parent = &e_root,
		.anchors = {
			.top = ANCHOR_TOP,
			.right = ANCHOR_RIGHT
		},
		.margins = {
			.top = 20
		},
		.width = 150
	},
	// FPS text image
	{
		.var = &e_text_fps,
		.parent = &e_hp_bar,
		.anchors = {
			.top = ANCHOR_BOTTOM,
			.left = ANCHOR_LEFT,
		},
		.margins = {
			.top = 10
		}
	},
	// render time text image
	{
		.var = &e_text_render_time,
		.parent = &e_text_fps,
		.anchors = {
			.top = ANCHOR_BOTTOM,
			.left = ANCHOR_LEFT,
		},
		.margins = {
			.top = 10
		},
	},
	// upgrades window
	{
		.var = &e_upgrades_win,
		.parent = &e_root,
		.anchors = {
			.hcenter = ANCHOR_HCENTER,
			.vcenter = ANCHOR_VCENTER
		},
		.width = 450,
		.height = 450
	},
	// upgrades window - weapons frame
	{
		.var = &e_upgrades_weapons_frame,
		.parent = &e_upgrades_win,
		.anchors = {
			.left = ANCHOR_LEFT,
			.right = ANCHOR_RIGHT,
			.top = ANCHOR_TOP,
		},
		.margins = {
			.top = 32,
			.left = 8,
			.right = 8,
		},
		.height = 100
	},
	{
		.var = NULL
	}
};

static int
on_click_upgrade_btn(void);

// Upgrade shop window buttons
static struct Button upgrades_win_buttons[] = {
	{ on_click_upgrade_btn }
};

static void
update_credits(int credits)
{
	text_set_fmt(text_credits, "Credits: %d$", credits);
	e_text_credits->height = text_credits->height;
}

static void
update_hitpoints(int hitpoints)
{
	hp_bar->width = (200.0 * hitpoints / PLAYER_INITIAL_HITPOINTS);
}

static int
ui_init(void)
{
	// initialize layout
	for (size_t i = 0; layout[i].var != NULL; i++) {
		struct Element *elem = *layout[i].var = element_new(
			layout[i].width, layout[i].height
		);
		if (!elem) {
			return 0;
		}

		if (layout[i].parent &&
		    !element_add_child(*layout[i].parent, elem)) {
			return 0;
		}

		elem->anchors = layout[i].anchors;
		elem->margins = layout[i].margins;
		elem->x = layout[i].x;
		elem->y = layout[i].y;
		elem->width = layout[i].width;
		elem->height = layout[i].height;
	}

	// create text renderables
	text_fps = text_new(font_dbg);
	text_render_time = text_new(font_dbg);
	text_credits = text_new(font_hud);
	if (!text_fps || !text_render_time || !text_credits) {
		return 0;
	}

	update_credits(0);

	// HP bar
	hp_bar = image_new();
	if (!hp_bar) {
		return 0;
	}
	hp_bar->texture = tex_hp_bar_green;
	hp_bar->border.left = 6;
	hp_bar->border.right = 6;

	hp_bar_bg = image_new();
	if (!hp_bar_bg) {
		return 0;
	}
	hp_bar_bg->texture = tex_hp_bar_bg;
	hp_bar_bg->border.left = 6;
	hp_bar_bg->border.right = 6;

	// upgrade shop window
	upgrades_win = image_new();
	if (!upgrades_win) {
		return 0;
	}
	upgrades_win->texture = tex_win;
	upgrades_win->border.left = 11;
	upgrades_win->border.right = 11;
	upgrades_win->border.top = 32;
	upgrades_win->border.bottom = 13;

	// upgrade shop weapon section frame
	upgrades_weapon_frame = image_new();
	upgrades_weapon_frame->texture = tex_frame;
	upgrades_weapon_frame->border.left = 7;
	upgrades_weapon_frame->border.right = 7;
	upgrades_weapon_frame->border.top = 7;
	upgrades_weapon_frame->border.bottom = 7;

	return 1;
}

int
ui_load(void)
{
	// load textures
	for (unsigned i = 0; textures[i].file != NULL; i++) {
		struct TextureRes res = textures[i];
		if (!(*res.var = texture_from_file(res.file))) {
			fprintf(
				stderr,
				"failed to load texture `%s`\n",
				res.file
			);
			return 0;
		}
		printf("loaded texure `%s`\n", res.file);
	}

	// load fonts
	for (unsigned i = 0; fonts[i].file != NULL; i++) {
		if (!(*fonts[i].var = font_from_file(fonts[i].file, fonts[i].size))) {
			fprintf(
				stderr,
				"failed to load font `%s`\n",
				fonts[i].file
			);
			return 0;
		}
		printf("loaded font `%s`\n", fonts[i].file);
	}

	return ui_init();
}

void
ui_cleanup(void)
{
	element_destroy(e_root);
	image_destroy(upgrades_win);
	image_destroy(hp_bar_bg);
	image_destroy(hp_bar);
	text_destroy(text_fps);
	text_destroy(text_render_time);
	text_destroy(text_credits);

	// destroy fonts
	for (unsigned i = 0; fonts[i].file; i++) {
		font_destroy(*fonts[i].var);
	}

	// destroy textures
	for (unsigned i = 0; textures[i].file; i++) {
		texture_destroy(*textures[i].var);
	}
}

int
ui_update(const struct State *state, float dt)
{
	// previous state, needed to compute diff between updates in order to
	// show changes only
	static struct State prev_state;
	static int first_update = 1;
	if (first_update) {
		memset(&prev_state, 0, sizeof(struct State));
		first_update = 0;
	}

	// update performance images once per second
	static float time_acc = 0;
	time_acc += dt;
	if (time_acc >= 1.0) {
		time_acc -= 1.0;
		text_set_fmt(text_fps, "FPS: %d", state->fps);
		e_text_fps->width = text_fps->width;
		e_text_fps->height = text_fps->height;

		text_set_fmt(
			text_render_time,
			"Render time: %dms",
			state->render_time
		);
		e_text_render_time->width = text_render_time->width;
		e_text_render_time->height = text_render_time->height;
	}

	// update credits text
	if (prev_state.credits != state->credits) {
		update_credits(state->credits);
	}

	// update hitpoints image
	if (prev_state.hitpoints != state->hitpoints) {
		update_hitpoints(state->hitpoints);
	}

	// update local UI state
	ui_state.show_upgrades_win = state->show_upgrades_shop;

	// compute the new layout
	if (!element_compute_layout(e_root)) {
		return 0;
	}

	// update images dimensions based on new layout metrics
	hp_bar->height = e_hp_bar->height;
	hp_bar_bg->width = e_hp_bar->width;
	hp_bar_bg->height = e_hp_bar->height;
	upgrades_win->width = e_upgrades_win->width;
	upgrades_win->height = e_upgrades_win->height;
	upgrades_weapon_frame->width = e_upgrades_weapons_frame->width;
	upgrades_weapon_frame->height = e_upgrades_weapons_frame->height;

	prev_state = *state;

	return 1;
}

void
ui_render(struct RenderList *rndr_list)
{
	// render FPS indicator
	render_list_add_text(
		rndr_list,
		text_fps,
		e_text_fps->x,
		e_text_fps->y
	);

	// render render time indicator
	render_list_add_text(
		rndr_list,
		text_render_time,
		e_text_render_time->x,
		e_text_render_time->y
	);

	// render credits counter
	render_list_add_text(
		rndr_list,
		text_credits,
		e_text_credits->x,
		e_text_credits->y
	);

	// render hitpoints image
	render_list_add_image(
		rndr_list,
		hp_bar_bg,
		e_hp_bar->x,
		e_hp_bar->y
	);
	render_list_add_image(
		rndr_list,
		hp_bar,
		e_hp_bar->x,
		e_hp_bar->y
	);

	// render upgrades shop window, if needed
	if (ui_state.show_upgrades_win) {
		render_list_add_image(
			rndr_list,
			upgrades_win,
			e_upgrades_win->x,
			e_upgrades_win->y
		);
		render_list_add_image(
			rndr_list,
			upgrades_weapon_frame,
			e_upgrades_weapons_frame->x,
			e_upgrades_weapons_frame->y
		);
	}
}

static int
dispatch_click(struct Button *buttons, unsigned count, int x, int y)
{
	for (size_t i = 0; i < count; i++) {
		int x1 = buttons[i].x, x2 = buttons[i].x + buttons[i].w;
		int y1 = buttons[i].y, y2 = buttons[i].y + buttons[i].h;

		if (x >= x1 && x <= x2 && y >= y1 && y <= y2) {
			if (!buttons[i].on_click()) {
				return 0;
			}
		}
	}
	return 1;
}

int
ui_handle_click(int x, int y)
{
	if (ui_state.show_upgrades_win) {
		return dispatch_click(
			upgrades_win_buttons,
			sizeof(upgrades_win_buttons) / sizeof(struct Button),
			x,
			y
		);
	}
	return 1;
}

static int
on_click_upgrade_btn(void)
{
	printf("Upgrade!\n");
	return 1;
}