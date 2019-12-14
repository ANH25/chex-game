#include <stdlib.h>
//#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_native_dialog.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>

#include "weighted-quick-union.h"


#define AL_RED al_map_rgb(255, 0, 0)
#define AL_BLUE al_map_rgb(0, 0, 255)
#define AL_BLACK al_map_rgb(0,0,0)
#define AL_WHITE al_map_rgb(255,255,255)


typedef enum cell_color {NEUTRAL = 0, RED, BLUE} cell_color;

typedef struct hex_grid {
	struct wqu_uf grid;
	size_t size;
	cell_color *cell_colors;
} hex_grid;

#define RED_VIRTUAL_CELLS_START(g) ((g)->size*(g)->size)
#define BLUE_VIRTUAL_CELLS_START(g) (RED_VIRTUAL_CELLS_START((g)) + 2)


#define DEF_GRID_SIZE 11
struct wqu_node grid_sites[DEF_GRID_SIZE*DEF_GRID_SIZE+4];
cell_color grid_cell_colors[DEF_GRID_SIZE*DEF_GRID_SIZE];

hex_grid def_grid = {.grid.nodes = grid_sites, .grid.size = DEF_GRID_SIZE*DEF_GRID_SIZE+4, 
.grid.count = DEF_GRID_SIZE*DEF_GRID_SIZE+4,
.size = DEF_GRID_SIZE, .cell_colors = grid_cell_colors 
};

void hex_def_grid_init(void) {
	
	for(size_t i = 0; i < DEF_GRID_SIZE*DEF_GRID_SIZE+4; i++) {
		grid_sites[i].id = i;
		grid_sites[i].size = 1;
	}
	memset(grid_cell_colors, 0, sizeof(cell_color) * DEF_GRID_SIZE*DEF_GRID_SIZE);
}

/*
int hex_grid_init_n(hex_grid *g, size_t size) {
	
	if(!w_quickunion_init(&g->grid, size*size + 4)) return 0;
	g->cell_colors = calloc(size*size, sizeof(cell_color));
	if(!g->cell_colors) {
		w_quickunion_destroy(&g->grid);
		return 0;
	}
	g->size = size;
	return 1;
}
*/


struct point {
	float x, y;
};

/* pointy_hex_corner() is stolen from https://www.redblobgames.com/grids/hexagons/#angles
   because I can't handle geometry :/
*/
#define PI 3.141593f
struct point pointy_hex_corner(struct point *center, float size, unsigned i) {
	
	float angle_deg = 60.0f * i - 30.0f;
	float angle_rad = PI / 180.0f * angle_deg;
	return (struct point) {.x = center->x + size * cosf(angle_rad), 
						   .y = center->y + size * sinf(angle_rad)};
	
}

void draw_filled_hexagon(float x, float y, float r, ALLEGRO_COLOR color) {
	
	float vertices[22];
	
	struct point center = {.x = x, .y = y};
	struct point corners[6];
	for(unsigned i = 0; i < 6; i++) {
		corners[5-i] = pointy_hex_corner(&center, r, i);
	}
	for(unsigned i = 0; i <= 16; i += 4) {
		vertices[i] = corners[i/4].x;
		vertices[i+1] = corners[i/4].y;
		vertices[i+2] = corners[i/4 + 1].x;
		vertices[i+3] = corners[i/4 + 1].y;
		
	}
	vertices[20] = corners[5].x;
	vertices[21] = corners[5].y;
	
	al_draw_filled_polygon(vertices, (sizeof(vertices)/sizeof(float))/2, color);
}

void draw_hexagon(float x, float y, float r, ALLEGRO_COLOR color) {
	
	float vertices[24];
	
	struct point center = {.x = x, .y = y};
	struct point corners[6];
	for(unsigned i = 0; i < 6; i++) {
		corners[i] = pointy_hex_corner(&center, r, i);
	}
	for(unsigned i = 0; i <= 16; i += 4) {
		vertices[i] = corners[i/4].x;
		vertices[i+1] = corners[i/4].y;
		vertices[i+2] = corners[i/4 + 1].x;
		vertices[i+3] = corners[i/4 + 1].y;
		
	}
	vertices[20] = corners[5].x;
	vertices[21] = corners[5].y;
	vertices[22] = corners[0].x;
	vertices[23] = corners[0].y;

	al_draw_polygon(vertices, (sizeof(vertices)/sizeof(float))/2, ALLEGRO_LINE_JOIN_BEVEL, color, 1.0f, 1.0f);
}


#define GRID_REGION_WIDTH(display) (al_get_display_width(display) * 0.9f)
#define GRID_REGION_HEIGHT(display) (al_get_display_height(display) * 0.9f)

#define SQRT_3 1.732051f
#define H_OFFSET(grid_size, width) ((width) / (grid_size) * 1.5f)
#define V_OFFSET(grid_size, height) ((height) / (grid_size))
#define CELL_SIZE(grid_size, width, h_offset) (0.5f * ((width) - (h_offset) * 2) / (grid_size))
#define CELL_WIDTH(cell_size) ((cell_size) * SQRT_3)
#define CELL_HEIGHT(cell_size) ((cell_size) * 2.0f)
#define CELL_X(h_offset, cell_width, i, j) ((h_offset) + (cell_width) * (j) + (cell_width)/2.0f * (i))
#define CELL_Y(v_offset, cell_height, i) ((v_offset) + 0.75f * (cell_height) * (i))

void hex_grid_draw(ALLEGRO_DISPLAY *display, hex_grid *g) {
	
	const float width = GRID_REGION_WIDTH(display);
	const float height = GRID_REGION_HEIGHT(display);
	const float h_offset = H_OFFSET(g->size, width);
	const float v_offset = V_OFFSET(g->size, height);
	const float cell_size = CELL_SIZE(g->size, width, h_offset);
	const float cell_width = CELL_WIDTH(cell_size);
	const float cell_height = CELL_HEIGHT(cell_size);
	
	/* grid borders */
	for (size_t j = 0; j < g->size-1; j++) {
		
		/* red borders */
		al_draw_filled_triangle(
		CELL_X(h_offset, cell_width, 0, j), 
		CELL_Y(v_offset, cell_height, 0) - cell_size, 
		CELL_X(h_offset, cell_width, 0, j+1), 
		CELL_Y(v_offset, cell_height, 0) - cell_size,
		CELL_X(h_offset, cell_width, 0, j) + cell_width/2.0f,
		CELL_Y(v_offset, cell_height, 0) - cell_size/2.0f,
		AL_RED);
		
		al_draw_filled_triangle(
		CELL_X(h_offset, cell_width, g->size-1, j), 
		CELL_Y(v_offset, cell_height, g->size-1) + cell_size, 
		CELL_X(h_offset, cell_width, g->size-1, j+1), 
		CELL_Y(v_offset, cell_height, g->size-1) + cell_size,
		CELL_X(h_offset, cell_width, g->size-1, j) + cell_width/2.0f,
		CELL_Y(v_offset, cell_height, g->size-1) + cell_size/2.0f,
		AL_RED);
		
		/* blue borders */
		al_draw_filled_triangle(
		CELL_X(h_offset, cell_width, j, 0) - cell_width/2.0f, 
		CELL_Y(v_offset, cell_height, j) + cell_height/4.0f, 
		CELL_X(h_offset, cell_width, j, 0), 
		CELL_Y(v_offset, cell_height, j) + cell_height/2.0f,
		CELL_X(h_offset, cell_width, j, 0),
		CELL_Y(v_offset, cell_height, j+1) + cell_height/4.0f,
		AL_BLUE);
		
		al_draw_filled_triangle(
		CELL_X(h_offset, cell_width, j, g->size-1) + cell_width/2.0f, 
		CELL_Y(v_offset, cell_height, j) - cell_height/4.0f, 
		CELL_X(h_offset, cell_width, j, g->size-1) + cell_width/2.0f, 
		CELL_Y(v_offset, cell_height, j) + cell_height/4.0f,
		CELL_X(h_offset, cell_width, j+1, g->size-1) + cell_width/2.0f,
		CELL_Y(v_offset, cell_height, j+1) - cell_height/4.0f,
		AL_BLUE);
	}
	
	/* grid cells */
	for (size_t i = 0; i < g->size; i++) {
		for (size_t j = 0; j < g->size; j++) {
			
			if (g->cell_colors[i*g->size + j] == RED) {
				draw_filled_hexagon(CELL_X(h_offset, cell_width, i, j), 
				CELL_Y(v_offset, cell_height, i), 
				cell_size, AL_RED);
			}
			else if (g->cell_colors[i*g->size + j] == BLUE) {
				draw_filled_hexagon(CELL_X(h_offset, cell_width, i, j), 
				CELL_Y(v_offset, cell_height, i), 
				cell_size, AL_BLUE);
			}
			else {
				draw_hexagon(CELL_X(h_offset, cell_width, i, j), 
				CELL_Y(v_offset, cell_height, i), 
				cell_size, AL_BLACK);
			}
		}
	}
	
}

size_t get_cell_index_from_mouse_coordinates(ALLEGRO_DISPLAY *display, hex_grid *g, int x, int y) {
	
	const float width = GRID_REGION_WIDTH(display);
	const float height = GRID_REGION_HEIGHT(display);
	const float h_offset = H_OFFSET(g->size, width);
	const float v_offset = V_OFFSET(g->size, height);
	const float cell_size = CELL_SIZE(g->size, width, h_offset);
	const float cell_width = CELL_WIDTH(cell_size);
	const float cell_height = CELL_HEIGHT(cell_size);
	
	float i_f = ((float)y - v_offset) / CELL_Y(0, cell_height, 1);
	size_t i = llroundf(i_f);
	float j_f = ((float)x - h_offset - (cell_width)/2.0f * i_f) / CELL_X(0, cell_width, 0, 1);
	size_t j = llroundf(j_f);
	//printf("i_f : %f , j_f : %f , i : %zu , j : %zu\n", i_f, j_f, i, j);
	if(i < g->size && j < g->size) {
		return i * g->size + j;
	}
	return -1;
}

#define HEXGAME_FIRST_PLAYER RED
cell_color current_color = HEXGAME_FIRST_PLAYER;

/* FIXME: Need more testing */
void open_cell(hex_grid *g, size_t i) {
	
	assert(i < g->size * g->size);
	
	if(g->cell_colors[i] != NEUTRAL) {
		return;
	}
	
	size_t x = i % g->size;
	size_t y = i / g->size;
	//printf("- opening cell[%zu] (%zu, %zu)\n", i, x, y);
	g->cell_colors[i] = current_color;
	
	size_t neighbors[6];
	neighbors[0] = i - g->size;
	neighbors[1] = (x != (g->size - 1) ? (i - g->size + 1) : (size_t)-1);
	neighbors[2] = (x != (g->size-1) ? (i + 1) : (size_t)-1);
	neighbors[3] = i + g->size;
	neighbors[4] = (x != 0 ? (i + g->size - 1) : (size_t)-1);
	neighbors[5] = (x != 0 ? (i - 1) : (size_t)-1);
	
	for(size_t n = 0; n < 6; n++) {
		if(neighbors[n] < g->size * g->size && 
		   g->cell_colors[neighbors[n]] == g->cell_colors[i])
		{
			w_quickunion_union(&g->grid, neighbors[n], i);
			//printf("- connecting (%zu, %zu) with neighbors[%zu] (%zu, %zu)\n", 
			//x, y, n, neighbors[n]%g->size, neighbors[n]/g->size);
		}
	}
	
	size_t red_idx = RED_VIRTUAL_CELLS_START(g);
	size_t blue_idx = BLUE_VIRTUAL_CELLS_START(g);
	
	if(y == 0 && current_color == RED) { // upper
		//printf("- connecting (%zu, %zu) with upper red corner\n", x, y);
		w_quickunion_union(&g->grid, red_idx, i);
	}
	else if(y == (g->size - 1) && current_color == RED) { // lower
		//printf("- connecting (%zu, %zu) with lower red corner\n", x, y);
		w_quickunion_union(&g->grid, red_idx+1, i);
	}
	else if(x == 0 && current_color == BLUE) { // left
		//printf("- connecting (%zu, %zu) with left blue corner\n", x, y);
		w_quickunion_union(&g->grid, blue_idx, i);
	}
	else if(x == (g->size-1) && current_color == BLUE) { // right
		//printf("- connecting (%zu, %zu) with right blue corner\n", x, y);
		w_quickunion_union(&g->grid, blue_idx+1, i);
	}
	
	if(current_color == RED) {
		current_color = BLUE;
	}
	else {
		current_color = RED;
	}
	
}

cell_color get_winner(hex_grid *g) {
	
	size_t red_idx = RED_VIRTUAL_CELLS_START(g);
	if(w_quickunion_is_connected(&g->grid, red_idx, red_idx+1)) return RED;
	size_t blue_idx = BLUE_VIRTUAL_CELLS_START(g);
	if(w_quickunion_is_connected(&g->grid, blue_idx, blue_idx+1)) return BLUE;
	return NEUTRAL;
}

void show_winner(ALLEGRO_DISPLAY *display, 
				ALLEGRO_FONT *font, ALLEGRO_FONT *font_big, 
				cell_color winner) {
	
	if(winner == BLUE) {
		al_draw_text(font_big, AL_BLUE, 
		50, 
		al_get_display_height(display) - 100,
		0, "Blue Wins!");
	}
	else if(winner == RED) {
		al_draw_text(font_big, AL_RED, 
		50, 
		al_get_display_height(display) - 100,
		0, "Red Wins!");
	}
	else return;
	
	al_draw_text(font, AL_BLACK, 
	50, 
	al_get_display_height(display) - 50,
	0, "Press R to play again");
		
}

void show_current_player(ALLEGRO_DISPLAY *display, ALLEGRO_FONT *font, cell_color player) {
	
	if(player == BLUE) {
		al_draw_text(font, AL_BLUE, 
		al_get_display_width(display) - 150, 
		50,
		0, "Blue To Move");
	}
	else if(player == RED) {
		al_draw_text(font, AL_RED, 
		al_get_display_width(display) - 150, 
		50,
		0, "Red To Move");
	}
}

struct hexgame_state {
	unsigned short redraw : 1;
	unsigned short reset : 1;
	unsigned short paused : 1;
	unsigned short game_ended : 1;
	unsigned short fullscreen : 1;
};

#define HEXGAME_FLAG_ON(flags, member) ((flags).member = ~((flags).member ^ (flags).member))
#define HEXGAME_FLAG_OFF(flags, member) ((flags).member = ((flags).member ^ (flags).member))
#define HEXGAME_FLIP_FLAG(flags, member) ((flags).member = ~(flags).member)


int main(int argc, char **argv) {

	al_init();
	al_init_primitives_addon();
	al_install_keyboard();
	al_install_mouse();
	al_init_font_addon();
	al_init_ttf_addon();
	al_init_image_addon();
	
	ALLEGRO_DISPLAY* display = al_create_display(600, 500);
	ALLEGRO_TIMER* timer = al_create_timer(1.0 / 30.0);
	ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
	ALLEGRO_FONT *font = al_load_ttf_font("VCR_OSD_MONO_1.001.ttf", 16, 0);
	if(!font) {
		font = al_create_builtin_font();
	}
	ALLEGRO_FONT *font_big = al_load_ttf_font("VCR_OSD_MONO_1.001.ttf", 32, 0);
	if(!font_big) {
		font_big = font;
	}
	ALLEGRO_BITMAP *icon = al_load_bitmap("icon.png");
	if(icon) {
		al_set_display_icon(display, icon);
	}
	
	
	al_register_event_source(queue, al_get_keyboard_event_source());
	al_register_event_source(queue, al_get_display_event_source(display));
	al_register_event_source(queue, al_get_timer_event_source(timer));
	al_register_event_source(queue, al_get_mouse_event_source());
	ALLEGRO_EVENT event;
	
	struct hexgame_state game_state;
	memset(&game_state, 0, sizeof(game_state));
	HEXGAME_FLAG_ON(game_state, redraw);
	HEXGAME_FLAG_ON(game_state, reset);
	
	cell_color winner = NEUTRAL;
	
	al_start_timer(timer);
	
	while (1)
	{
		al_wait_for_event(queue, &event);

		if(event.type == ALLEGRO_EVENT_TIMER && !game_state.paused)
		{
			HEXGAME_FLAG_ON(game_state, redraw);
		}
		else if((event.type == ALLEGRO_EVENT_KEY_DOWN && 
				event.keyboard.keycode == ALLEGRO_KEY_ESCAPE) || 
				(event.type == ALLEGRO_EVENT_DISPLAY_CLOSE)) 
		{
			break;
		}
		else if(event.type == ALLEGRO_EVENT_KEY_DOWN &&
				event.keyboard.keycode == ALLEGRO_KEY_P)
		{
			HEXGAME_FLIP_FLAG(game_state, paused);
		}
		else if(event.type == ALLEGRO_EVENT_KEY_DOWN &&
				event.keyboard.keycode == ALLEGRO_KEY_F)
		{
			HEXGAME_FLIP_FLAG(game_state, fullscreen);
			al_set_display_flag(display, ALLEGRO_FULLSCREEN_WINDOW, game_state.fullscreen);
		}
		else if(event.type == ALLEGRO_EVENT_KEY_DOWN &&
				event.keyboard.keycode == ALLEGRO_KEY_R)
		{
			HEXGAME_FLAG_ON(game_state, reset);
		}
		else if(event.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN &&
				event.mouse.button == 1)
		{
			if(!game_state.game_ended) {
				//printf("left mouse button clicked on x: %d , y: %d\n", event.mouse.x, event.mouse.y);
				size_t i = get_cell_index_from_mouse_coordinates(display, &def_grid, 
						event.mouse.x, event.mouse.y);			
				if(i != (size_t)-1) {
					open_cell(&def_grid, i);
					winner = get_winner(&def_grid);
					if(winner == RED) {
						//puts("- red wins!");
						HEXGAME_FLAG_ON(game_state, game_ended);
					}
					else if(winner == BLUE) {
						//puts("- blue wins!");
						HEXGAME_FLAG_ON(game_state, game_ended);
					}
				}
			}
		}

		if(game_state.redraw && al_is_event_queue_empty(queue))
		{
			if (game_state.reset) {
				hex_def_grid_init();
				HEXGAME_FLAG_OFF(game_state, reset);
				HEXGAME_FLAG_OFF(game_state, game_ended);
				winner = NEUTRAL;
				current_color = HEXGAME_FIRST_PLAYER;
			}
			al_clear_to_color(AL_WHITE);
			hex_grid_draw(display, &def_grid);
			show_current_player(display, font, current_color);
			show_winner(display, font, font_big, winner);
			al_flip_display();
			HEXGAME_FLAG_OFF(game_state, redraw);
		}
	}
	
	return 0;
}
