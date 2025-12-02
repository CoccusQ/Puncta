#ifndef COC_CANVAS_H
#define COC_CANVAS_H

#include "coc.h"

#define coc_swap(a, b, T) do { T temp = a; a = b; b = temp; } while(0)
#define RGB(r, g, b) ((const struct Coc_Color){(r), (g), (b)})
#define COLOR_MAX 255
#define COC_CANVAS_SIZE 32
#define COC_PIXEL_SIZE 16
typedef struct Coc_Color {
    uint8_t r, g, b;
} Coc_Color;

typedef Coc_Color Coc_Canvas[COC_CANVAS_SIZE][COC_CANVAS_SIZE];

#define RED     (Coc_Color){ 255,   0,   0 }
#define GREEN   (Coc_Color){   0, 255,   0 }
#define BLUE    (Coc_Color){   0,   0, 255 }
#define BLACK   (Coc_Color){   0,   0,   0 }
#define WHITE   (Coc_Color){ 255, 255, 255 }
#define YELLOW  (Coc_Color){ 255, 255,   0 }
#define PURPLE  (Coc_Color){ 128,   0, 128 }
#define CYAN    (Coc_Color){   0, 255, 255 }
#define MAGENTA (Coc_Color){ 255,   0, 255 }
#define ORANGE  (Coc_Color){ 255, 165,   0 }
#define GRAY    (Coc_Color){ 128, 128, 128 }

static inline void coc_canvas_clear(Coc_Canvas canvas, Coc_Color color) {
    for (int i = 0; i < COC_CANVAS_SIZE; i++)
        for (int j = 0; j < COC_CANVAS_SIZE; j++)
            canvas[i][j] = color;
}

static inline FILE *coc_canvas_init(Coc_Canvas canvas, const char* filename) {
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        coc_log(COC_ERROR, "Cannot open file %s: %s", filename, strerror(errno));
        exit(1);
    }
    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n", COC_CANVAS_SIZE * COC_PIXEL_SIZE, COC_CANVAS_SIZE * COC_PIXEL_SIZE);
    fprintf(fp, "%d\n", COLOR_MAX);
    return fp;
}

static inline void coc_canvas_save(Coc_Canvas canvas, FILE *fp) {
    for (int y = 0; y < COC_CANVAS_SIZE; y++) {
        for (int i = 0; i < COC_PIXEL_SIZE; i++) {
            for (int x = 0; x < COC_CANVAS_SIZE; x++) {
                for (int j = 0; j < COC_PIXEL_SIZE; j++) {
                    uint8_t r = canvas[y][x].r;
                    uint8_t g = canvas[y][x].g;
                    uint8_t b = canvas[y][x].b;
                    fputc(r, fp);
                    fputc(g, fp);
                    fputc(b, fp);
                }
            }
        }
    }
}

static inline void coc_canvas_close(Coc_Canvas canvas, FILE *fp) {
    coc_canvas_save(canvas, fp);
    fclose(fp);
}

static inline void coc_set_pixel(Coc_Canvas canvas, int x, int y, Coc_Color color) {
    if ((y) < 0 || (y) >= COC_CANVAS_SIZE ||
        (x) < 0 || (x) >= COC_CANVAS_SIZE) {
        return;
    }
    canvas[y][x] = color;
}

static inline void coc_line(Coc_Canvas canvas, int x1, int y1, int x2, int y2, Coc_Color color) {
    int dx = x2 - x1, dy = y2 - y1;
    int sign_x = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    int sign_y = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    dx = abs(dx), dy = abs(dy);
    bool change = false;
    if (dy > dx) {
        coc_swap(dx, dy, int);
        change = true;
    }
    int e = -dx;
    int x = x1, y = y1;
    for (int i = 0; i <= dx; i++) {
        coc_set_pixel(canvas, x, y, color);
        e += 2 * dy;
        if (e >= 0) {
            if (change) x += sign_x;
            else y += sign_y;
            e -= 2 * dx;
        }
        if (change) y += sign_y;
        else x += sign_x;
    }
}

static inline void coc_circle(Coc_Canvas canvas, int x, int y, int radius, Coc_Color color) {
    radius = abs(radius);
    int dx = 0, dy = radius;
    int e = 1 - radius;
    while (dx <= dy) {
        coc_set_pixel(canvas, x + dx, y + dy, color);
        coc_set_pixel(canvas, x + dx, y - dy, color);
        coc_set_pixel(canvas, x - dx, y + dy, color);
        coc_set_pixel(canvas, x - dx, y - dy, color);
        coc_set_pixel(canvas, x + dy, y + dx, color);
        coc_set_pixel(canvas, x + dy, y - dx, color);
        coc_set_pixel(canvas, x - dy, y + dx, color);
        coc_set_pixel(canvas, x - dy, y - dx, color);
        if (e < 0) e += 2 * (dx++) + 1;
        else e += 2 * ((dx++) - (dy--)) + 1;
    }
}

#endif