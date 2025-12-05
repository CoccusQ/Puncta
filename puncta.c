#include "puncta.h"
#include "coc_canvas.h"

Coc_Canvas canvas = {0};
Coc_Color draw_color = BLACK;

void act_gfx_plot(Number *n) {
    long long pos = n->is_float ? (long long)n->float_value : n->int_value;
    int x = pos / COC_CANVAS_SIZE;
    int y = pos % COC_CANVAS_SIZE;
    coc_set_pixel(canvas, x, y, draw_color); 
}

void act_gfx_color(Number *n) {
    uint32_t c = n->is_float ? (uint32_t)n->float_value : (uint32_t)n->int_value;
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = c & 0xFF;
    draw_color = RGB(r, g, b);
}

void act_gfx_clear(Number *n) {
    uint32_t c = n->is_float ? (uint32_t)n->float_value : (uint32_t)n->int_value;
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = c & 0xFF;
    coc_canvas_clear(canvas, RGB(r, g, b));
}

void act_gfx_up(Number *n) {
    long long pos = n->is_float ? (long long)n->float_value : n->int_value;
    int x = pos / COC_CANVAS_SIZE;
    int y = pos % COC_CANVAS_SIZE;
    y = (y - 1 + COC_CANVAS_SIZE) % COC_CANVAS_SIZE;
    n->int_value = x * COC_CANVAS_SIZE + y;
    coc_set_pixel(canvas, x, y, draw_color); 
}

void act_gfx_down(Number *n) {
    long long pos = n->is_float ? (long long)n->float_value : n->int_value;
    int x = pos / COC_CANVAS_SIZE;
    int y = pos % COC_CANVAS_SIZE;
    y = (y + 1) % COC_CANVAS_SIZE;
    n->int_value = x * COC_CANVAS_SIZE + y;
    coc_set_pixel(canvas, x, y, draw_color); 
}

void act_gfx_left(Number *n) {
    long long pos = n->is_float ? (long long)n->float_value : n->int_value;
    int x = pos / COC_CANVAS_SIZE;
    int y = pos % COC_CANVAS_SIZE;
    x = (x - 1 + COC_CANVAS_SIZE) % COC_CANVAS_SIZE;
    n->int_value = x * COC_CANVAS_SIZE + y;
    coc_set_pixel(canvas, x, y, draw_color); 
}

void act_gfx_right(Number *n) {
    long long pos = n->is_float ? (long long)n->float_value : n->int_value;
    int x = pos / COC_CANVAS_SIZE;
    int y = pos % COC_CANVAS_SIZE;
    x = (x + 1) % COC_CANVAS_SIZE;
    n->int_value = x * COC_CANVAS_SIZE + y;
    coc_set_pixel(canvas, x, y, draw_color); 
}

static inline void register_user_actions(VM *vm) {
    register_act(vm, "plot",  act_gfx_plot);
    register_act(vm, "color", act_gfx_color);
    register_act(vm, "clear", act_gfx_clear);
    register_act(vm, "up",    act_gfx_up);
    register_act(vm, "down",  act_gfx_down);
    register_act(vm, "left",  act_gfx_left);
    register_act(vm, "right", act_gfx_right);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        coc_log(COC_ERROR, "Usage: %s <file name>", argv[0]);
        return 1;
    }
    FILE *fp = coc_canvas_init(canvas, "output.ppm");
    run_file(argv[1], register_user_actions);
    coc_canvas_save(canvas, fp);
    return 0;
}