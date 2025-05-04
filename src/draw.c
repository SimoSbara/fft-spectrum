#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "draw.h"

struct rgb
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

long kitty_id;
int w, h;
uint8_t *fb = NULL;
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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

void kitty_draw_sound(int frame, double complex* buffer, int samples, double min, double max)
{
    double range = max - min;

    if(range == 0 || buffer == NULL || samples <= 1)
        return;

    double mag1, mag2;
    int x1, x2, y1, y2;

    memset(fb, 0, w * h * 3);

    for(int i = 0; i < samples - 1; i++)
    {
        mag1 = (cabs(buffer[i]) - min) / range;
        mag2 = (cabs(buffer[i + 1]) - min) / range;
        
        x1 = i;
        x2 = i + 1;
        y1 = (double)(h - 1) - mag1 * (double)(h - 1);
        y2 = (double)(h - 1) - mag2 * (double)(h - 1);

        draw_line(x1, x2, y1, y2);
    }

    kitty_update_display(0, w, h);
}

void draw_line(int x1, int x2, int y1, int y2)
{
    struct rgb* ptr = fb, *pixel;
    int x, y;

    // line between two points
    int dx = x2 - x1;
    int dy = y2 - y1;
    double m = dy/dx;

    for(x = x1; x <= x2; x++)
    {
        y = m * (x - x1) + y1;

        pixel = ptr + y * w + x;

        pixel->r = 0;
        pixel->g = 255;
        pixel->b = 0;
    }
}