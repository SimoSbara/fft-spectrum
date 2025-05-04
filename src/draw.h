#include <complex.h>

//kitty draw
void kitty_init(int width, int height);
void kitty_stop();
void kitty_update_display(int frame, int width, int height);
void kitty_draw_sound(int frame, double complex* buffer, int samples, double min, double max);

// help functions
void draw_line(int x1, int x2, int y1, int y2);