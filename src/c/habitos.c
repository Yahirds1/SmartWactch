#include <pebble.h>

// ---------------------------------------------------------------
// Rastreador de habitos para Pebble - version con diseno mejorado
//  - SELECT: marcar / desmarcar el habito de hoy
//  - UP / DOWN: moverse por la lista
//  - Guarda el estado en el reloj y calcula la racha (dias seguidos)
// Para cambiar los habitos, edita HABIT_NAMES y NUM_HABITS.
// ---------------------------------------------------------------

#define NUM_HABITS 5
#define PERSIST_KEY 1

#define ROW_HEIGHT 44
#define HEADER_HEIGHT 34
#define PAD PBL_IF_ROUND_ELSE(24, 8)

#ifdef PBL_COLOR
  #define COLOR_ACCENT GColorIslamicGreen
  #define COLOR_HEADER GColorOxfordBlue
  #define COLOR_BAR    GColorGreen
  #define COLOR_STREAK GColorOrange
  #define COLOR_MUTED  GColorDarkGray
#else
  #define COLOR_ACCENT GColorBlack
  #define COLOR_HEADER GColorBlack
  #define COLOR_BAR    GColorWhite
  #define COLOR_STREAK GColorBlack
  #define COLOR_MUTED  GColorBlack
#endif

static const char *HABIT_NAMES[NUM_HABITS] = {
  "Tomar agua",
  "Ejercicio",
  "Leer 10 min",
  "Meditar",
  "Dormir temprano"
};

typedef struct {
  uint16_t streak;   // dias seguidos
  int32_t last_day;  // ultimo dia en que se marco (0 = nunca)
} HabitState;

static Window *s_window;
static MenuLayer *s_menu;
static HabitState s_habits[NUM_HABITS];

// ---------- Utilidades de fecha ----------

static int32_t day_id(time_t t) {
  struct tm *lt = localtime(&t);
  return lt->tm_year * 400 + lt->tm_yday;
}

static int32_t today_id(void) {
  return day_id(time(NULL));
}

static int32_t yesterday_id(void) {
  return day_id(time(NULL) - 86400);
}

// ---------- Estado ----------

static void refresh_streaks(void) {
  int32_t today = today_id();
  int32_t yesterday = yesterday_id();
  for (int i = 0; i < NUM_HABITS; i++) {
    int32_t d = s_habits[i].last_day;
    if (d != today && d != yesterday) {
      s_habits[i].streak = 0;  // se rompio la racha
    }
  }
}

static void load_state(void) {
  memset(s_habits, 0, sizeof(s_habits));
  if (persist_exists(PERSIST_KEY)) {
    persist_read_data(PERSIST_KEY, s_habits, sizeof(s_habits));
  }
  refresh_streaks();
}

static void save_state(void) {
  persist_write_data(PERSIST_KEY, s_habits, sizeof(s_habits));
}

static int done_count(void) {
  int32_t today = today_id();
  int n = 0;
  for (int i = 0; i < NUM_HABITS; i++) {
    if (s_habits[i].last_day == today) n++;
  }
  return n;
}

static void toggle_habit(int i) {
  HabitState *h = &s_habits[i];
  int32_t today = today_id();
  int32_t yesterday = yesterday_id();

  if (h->last_day == today) {
    // Desmarcar
    if (h->streak > 0) h->streak--;
    h->last_day = (h->streak > 0) ? yesterday : 0;
  } else {
    // Marcar
    h->streak = (h->last_day == yesterday) ? h->streak + 1 : 1;
    h->last_day = today;
    vibes_short_pulse();
  }
  save_state();
}

// ---------- Dibujo ----------

static uint16_t get_num_rows(MenuLayer *layer, uint16_t section, void *data) {
  return NUM_HABITS;
}

static int16_t get_header_height(MenuLayer *layer, uint16_t section, void *data) {
  return HEADER_HEIGHT;
}

static int16_t get_cell_height(MenuLayer *layer, MenuIndex *idx, void *data) {
  return ROW_HEIGHT;
}

static void draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  GRect b = layer_get_bounds(cell);
  int done = done_count();
  int w = b.size.w - 2 * PAD;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  // Fondo
  graphics_context_set_fill_color(ctx, COLOR_HEADER);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Titulo (izquierda) y contador (derecha)
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, done == NUM_HABITS ? "Todo listo!" : "Mis habitos", font,
                     GRect(PAD, 0, w, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  char buf[8];
  snprintf(buf, sizeof(buf), "%d/%d", done, NUM_HABITS);
  graphics_draw_text(ctx, buf, font, GRect(PAD, 0, w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  // Barra de progreso
  GRect track = GRect(PAD, 25, w, 5);
  int fill = w * done / NUM_HABITS;
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, track, 2, GCornersAll);
  if (fill > 0) {
    graphics_context_set_fill_color(ctx, COLOR_BAR);
    graphics_fill_rect(ctx, GRect(PAD, 25, fill, 5), 2, GCornersAll);
  }
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_rect(ctx, track);
  if (fill > 0) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(PAD, 25, fill, 5), 0, GCornerNone);
  }
#endif
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  const HabitState *h = &s_habits[idx->row];
  bool done = (h->last_day == today_id());
  bool hl = menu_cell_layer_is_highlighted(cell);
  GRect b = layer_get_bounds(cell);

  // Fondo (verde si esta seleccionada)
  graphics_context_set_fill_color(ctx, hl ? COLOR_ACCENT : GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Casilla circular
  const int r = 10;
  GPoint c = GPoint(PAD + r, b.size.h / 2);
  graphics_context_set_stroke_width(ctx, 2);
  if (done) {
    graphics_context_set_fill_color(ctx, hl ? GColorWhite : COLOR_ACCENT);
    graphics_fill_circle(ctx, c, r);
    graphics_context_set_stroke_color(ctx, hl ? COLOR_ACCENT : GColorWhite);
    graphics_draw_line(ctx, GPoint(c.x - 5, c.y), GPoint(c.x - 1, c.y + 4));
    graphics_draw_line(ctx, GPoint(c.x - 1, c.y + 4), GPoint(c.x + 5, c.y - 4));
  } else {
    graphics_context_set_stroke_color(ctx, hl ? GColorWhite : COLOR_MUTED);
    graphics_draw_circle(ctx, c, r);
  }
  graphics_context_set_stroke_width(ctx, 1);

  // Texto: nombre y racha
  int tx = PAD + 2 * r + 10;
  int tw = b.size.w - tx - PAD;

  GColor name_color = hl ? GColorWhite : (done ? COLOR_MUTED : GColorBlack);
  graphics_context_set_text_color(ctx, name_color);
  graphics_draw_text(ctx, HABIT_NAMES[idx->row],
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(tx, 2, tw, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  char sub[24];
  if (h->streak > 0) {
    snprintf(sub, sizeof(sub), "Racha: %d %s", (int)h->streak,
             h->streak == 1 ? "dia" : "dias");
  } else {
    snprintf(sub, sizeof(sub), "Sin racha");
  }
  GColor sub_color = hl ? GColorWhite : (h->streak > 0 ? COLOR_STREAK : COLOR_MUTED);
  graphics_context_set_text_color(ctx, sub_color);
  graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(tx, 24, tw, 18), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

#ifdef PBL_COLOR
  // Linea separadora suave
  if (!hl) {
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_draw_line(ctx, GPoint(PAD, b.size.h - 1), GPoint(b.size.w - PAD, b.size.h - 1));
  }
#endif
}

static void select_click(MenuLayer *layer, MenuIndex *idx, void *data) {
  toggle_habit(idx->row);
  menu_layer_reload_data(layer);
}

// ---------- Ventana ----------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = get_num_rows,
    .get_header_height = get_header_height,
    .get_cell_height = get_cell_height,
    .draw_header = draw_header,
    .draw_row = draw_row,
    .select_click = select_click,
  });
#ifdef PBL_COLOR
  menu_layer_set_highlight_colors(s_menu, COLOR_ACCENT, GColorWhite);
#endif
#ifdef PBL_ROUND
  menu_layer_set_center_focused(s_menu, true);
#endif
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
}

static void init(void) {
  load_state();
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}