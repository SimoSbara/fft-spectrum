#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>

#include "draw.h"

long kitty_id;
int w, h;
uint8_t *fb = NULL;

//from antirez implementation in c64-kitty
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

//from antirez implementation in c64-kitty
size_t base64_encode(const unsigned char *data, size_t input_length, char *encoded_data) {
    size_t output_length = 4 * ((input_length + 2) / 3);
    size_t i, j;

    for (i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded_data[j++] = base64_table[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 12) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 6) & 0x3F];
        encoded_data[j++] = base64_table[triple & 0x3F];
    }

    // Add padding if needed
    size_t mod_table[] = {0, 2, 1};
    for (i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[output_length - 1 - i] = '=';

    return output_length;
}

//from antirez implementation in c64-kitty
// Initialize Kitty graphics protocol
void kitty_init(int width, int height) 
{
    // Initialize random seed for image ID
    srand(time(NULL));
    kitty_id = rand();

    // Allocate framebuffer memory
    fb = malloc(width * height * 3);
    memset(fb, 0, width * height * 3);

    w = width;
    h = height;
}

void kitty_stop() 
{
    if(fb)
    {
        free(fb);
        fb = NULL;
    }
}

//from antirez implementation in c64-kitty
void kitty_update_display(int frame, int width, int height) 
{
    // Calculate base64 encoded size
    size_t bitmap_size = width * height * 3;
    size_t encoded_size = 4 * ((bitmap_size + 2) / 3);
    char *encoded_data = (char*)malloc(encoded_size + 1);

    if (!encoded_data) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    // Encode the bitmap data to base64
    base64_encode(fb, bitmap_size, encoded_data);
    encoded_data[encoded_size] = '\0';  // Null-terminate the string

    // Send Kitty Graphics Protocol escape sequence with base64 data
    printf("\033_Ga=%c,i=%lu,f=24,s=%d,v=%d,q=2,c=30,r=10;", frame == 0 ? 'T' : 't',  kitty_id, width, height);
    printf("%s", encoded_data);
    printf("\033\\");
    if (frame == 0) printf("\r\n");
    fflush(stdout);

    // Clean up
    free(encoded_data);
}

void kitty_draw_fft(int frame, int16_t* buffer, int samples, double min, double max)
{
    double x, mag, range = max - min;
    int y1, y2;
    struct rgb color;

    if(range == 0 || buffer == NULL || samples <= 1)
        return;

    double coeff = (double)w / (double)samples;
    int h1 = h - 1;  
    
    memset(fb, 0, w * h * 3);

    //magenta
    color.r = 255;
    color.g = 0;
    color.b = 255;

    y1 = h1;
    x = 0;

    for(int i = 0; i < samples; i++, x += coeff)
    {
        mag = fmax(fmin((buffer[i] - min) / range, 1), 0);
        y2 = h1 - mag * h1;

        draw_line(x, x, y1, y2, color);
    }

    kitty_update_display(frame, w, h);
}

void kitty_draw_sound(int frame, int16_t* buffer, int samples, double min, double max)
{
    double x, mag, range = max - min;
    int y1, y2;
    struct rgb color;
    
    if(range == 0 || buffer == NULL || samples <= 1)
        return;

    double coeff = (double)w / (double)samples;
    double h1 = h - 1;
    
    memset(fb, 0, w * h * 3);
    
    color.r = 0;
    color.g = 255;
    color.b = 0;

    y1 = h / 2;
    x = 0;

    for(int i = 0; i < samples; i++, x += coeff)
    {
        mag = fmax(fmin((buffer[i] - min) / range, 1), 0);
        y2 = mag * h1;

        draw_line(x, x, y1, y2, color);
    }

    kitty_update_display(frame, w, h);
}

void draw_line(int x1, int x2, int y1, int y2, struct rgb color)
{
    struct rgb* ptr = fb, *pixel;
    int x, y;

    // line between two points
    int dx = x2 - x1;

    if(y1 > y2)
    {
	int tmp = y1;
    	y1 = y2;
        y2 = tmp;
    }

    if(dx == 0)
    {
        x = x1;

        pixel = ptr + y1 * w + x; 	

        for(y = y1; y <= y2; y++)
        {
            pixel->r = color.r;
            pixel->g = color.g;
            pixel->b = color.b;

            pixel += w;
        }
    }
    else
    {
        int dy = y2 - y1;
        double m = dy/dx;

        for(x = x1; x <= x2; x++)
        {
            y = m * (x - x1) + y1;

            pixel = ptr + y * w + x;

            pixel->r = color.r;
            pixel->g = color.g;
            pixel->b = color.b;
        }
    }
}
