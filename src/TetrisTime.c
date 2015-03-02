#include <pebble.h>
#include "digit.h"
#include "field.h"
  
#define ANIMATION_TIMEOUT_MS 100
#define BACKGROUND_COLOR GColorWhite
#define DIGIT_COUNT 4

typedef struct {
    DigitDef target;
    DigitDef current;
    int offset_x;
    int offset_y;
    int value;
} DigitState;

static Window *s_window;
static Layer *s_layer;

DigitState s_states[DIGIT_COUNT];

static void draw_digit_def(const DigitDef* def, Layer *layer, GContext *ctx, int offset_x, int offset_y) {
    for (int i = 0; i < def->size; ++i) {
        const TetriminoPos* tp = &def->tetriminos[i];
        const TetriminoDef* td = get_tetrimino_def(tp->letter); 
        const TetriminoMask* tm = &td->rotations[tp->rotation];

        for (int mask_y = 0; mask_y < TETRIMINO_MASK_SIZE; ++mask_y) {
            for (int mask_x = 0; mask_x < TETRIMINO_MASK_SIZE; ++mask_x) {
                if ((*tm)[mask_y][mask_x]) {
                    const int x = tp->x + mask_x + offset_x;
                    const int y = tp->y + mask_y + offset_y;
                    field_draw(x, y, GColorBlack);
                }
            }
        }
    }
}

static void state_step(DigitState* state) {
    if (state->target.size == 0) {
        return;
    }

    const int start_y = -TETRIMINO_MASK_SIZE * 2;
    
    int last_y = TETRIMINO_MASK_SIZE;
    for (int i = 0; i < state->current.size; ++i) {
        TetriminoPos* current_pos = &state->current.tetriminos[i];
        const TetriminoPos* target_pos = &state->target.tetriminos[i];
        if (current_pos->rotation != target_pos->rotation) {
            current_pos->rotation = (current_pos->rotation + 1) % 4;
        }
        if (current_pos->y < target_pos->y) {
            current_pos->y += 1;
        } 
        if (current_pos->x < target_pos->x) {
            current_pos->x += 1;
        } else if (current_pos->x > target_pos->x) {
            current_pos->x -= 1;
        }
        last_y = current_pos->y;
    }
    if (last_y >= (start_y + TETRIMINO_MASK_SIZE)) {
        if (state->current.size < state->target.size) {
            const char target_letter = state->target.tetriminos[state->current.size].letter;
            const TetriminoDef* td = get_tetrimino_def(target_letter); 
            TetriminoPos* current_pos = &state->current.tetriminos[state->current.size];
            current_pos->letter = target_letter;
            current_pos->x = rand() % (DIGIT_WIDTH - td->size + 1);
            current_pos->y = start_y;
            current_pos->rotation = rand() % 4;
            state->current.size += 1;
        }
    }
}


static void layer_draw(Layer *layer, GContext *ctx) {
    for (int i = 0; i < DIGIT_COUNT; ++i) {
        draw_digit_def(&s_states[i].current, layer, ctx, s_states[i].offset_x, s_states[i].offset_y);
    }
    field_flush(layer, ctx, BACKGROUND_COLOR);
}

static void process_animation(void* data) {
    for (int i = 0; i < DIGIT_COUNT; ++i) {
        state_step(&s_states[i]);
    }
    layer_mark_dirty(s_layer);
    app_timer_register(ANIMATION_TIMEOUT_MS, process_animation, NULL);
}

static void main_window_load(Window *window) {
    s_layer = window_get_root_layer(window);

    field_init(BACKGROUND_COLOR);
    layer_set_update_proc(s_layer, layer_draw);
}

static void main_window_unload(Window *window) {
}

static void tick_handler(struct tm* tick_time, TimeUnits units_changed) {
    int digit_values[4];
    digit_values[0] = tick_time->tm_hour / 10;
    digit_values[1] = tick_time->tm_hour % 10;
    digit_values[2] = tick_time->tm_min / 10;
    digit_values[3] = tick_time->tm_min % 10;

    for (int i = 0; i < 4; ++i) {
        int value = digit_values[i];
        if (s_states[i].value != value) {
            s_states[i].value = value;
            reorder_digit(&s_states[i].target, &s_digits[value]);
            s_states[i].current.size = 0;
        }
    }
}
  
static void init() {
    srand(time(NULL));
    
#if USE_RAW_DIGITS == 1
    for (int i = 0; i < 10; ++i) {
        if (!parse_raw_digit(&s_digits[i], &s_raw_digits[i])) {
            return;
        }
    }
#endif

    for (int i = 0; i < 4; ++i) {
        s_states[i].value = 10;
        s_states[i].offset_x = i * 7 + 1;
        s_states[i].offset_y = 10;
    }
  
    // Create main Window element and assign to pointer
    s_window = window_create();

    // Set handlers to manage the elements inside the Window
    window_set_window_handlers(s_window, (WindowHandlers) {
            .load = main_window_load,
                .unload = main_window_unload
                });

    // Show the Window on the watch, with animated=true
    window_stack_push(s_window, true);
  
    // Register with TickTimerService
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    app_timer_register(ANIMATION_TIMEOUT_MS, process_animation, NULL);
}

static void deinit() {
    // Destroy Window
    window_destroy(s_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
