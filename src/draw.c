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

    if(range == 0 || !buffer || samples <= 0)
        return;

    double mag;
    int x, y;

    struct rgb* ptr = fb;
    struct rgb* pixel;

    memset(fb, 0, w * h * 3);

    for(int i = 0; i < samples; i++)
    {
        mag = (cabs(buffer[i]) - min) / range;
        
        x = i;
        y = (double)(h - 1) - mag * (double)(h - 1);

        //printf("x y mag %d %d %f\n", x, y);

        pixel = ptr + y * w + x;

        pixel->r = 0;
        pixel->g = 255;
        pixel->b = 0;
    }

    kitty_update_display(0, w, h);
}