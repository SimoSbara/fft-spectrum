#include <complex.h>

struct rgb
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

//kitty draw
void kitty_init(int width, int height);
void kitty_stop();
void kitty_update_display(int frame, int width, int height);
void kitty_draw_sound(int frame, int16_t* buffer, int samples, double min, double max); //wave
void kitty_draw_fft(int frame, int16_t* buffer, int samples, double min, double max); //magnitudes

// help functions
void draw_line(int x1, int x2, int y1, int y2, struct rgb color);
