#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <assert.h>
#include <math.h>
#include <alsa/asoundlib.h>

#include "fft.h"

long kitty_id;
uint8_t *fb;
double complex *signal;

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

struct rgb
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

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

void init_signal(int samples)
{
    signal = malloc(samples * sizeof(double complex));

    double pi = acos(-1);
    double coeff = 2.0 * pi / (double)samples;
    double c = 0;

    for(int i = 0; i < samples; i++, c += coeff)
    {
        signal[i] = sin(c) + cos(2 * c + pi / 3) + sin(5 * c - pi / 6);
    }
}

// Initialize Kitty graphics protocol
void kitty_init(int width, int height) {
    // Initialize random seed for image ID
    srand(time(NULL));
    kitty_id = rand();

    // Allocate framebuffer memory
    fb = malloc(width * height * 3);
    memset(fb, 0, width * height * 3);
}

void kitty_update_display(int frame, int width, int height) {
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

#define PCM_DEVICE "default"

snd_pcm_t *pcm_handle;
snd_pcm_hw_params_t *params;
snd_pcm_uframes_t frames;
unsigned int sample_rate = 44100;
int dir;

uint16_t *mic_buffer;
int size;

//open microphone
void init_microphone()
{
    int rc = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) 
    {
        fprintf(stderr, "Error opening device PCM: %s\n", snd_strerror(rc));
        exit(1);
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, params, 1);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &sample_rate, &dir);

    // Imposta i parametri hardware
    rc = snd_pcm_hw_params(pcm_handle, params);
    if (rc < 0) 
    {
        fprintf(stderr, "Error setting parameters: %s\n", snd_strerror(rc));
        exit(1);
    }

    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * 2; //16 bit
    mic_buffer = (uint16_t*)malloc(size);
}

int main(int argc, char* argv[])
{
    int w = 128;
    int h = 128;

    kitty_init(w, h);
    init_microphone();

    int n = frames;

    init_signal(n);

    int f = 0, frame = 0, inverse = 0;
    double max, min, range;

    while(f < 20)
    {
        int rc = snd_pcm_readi(pcm_handle, mic_buffer, frames);
        if (rc == -EPIPE) {
            fprintf(stderr, "overflow!\n");
            snd_pcm_prepare(pcm_handle);
        } else if (rc < 0) {
            fprintf(stderr, "error microphone: %s\n", snd_strerror(rc));
        } else if (rc != (int)frames) {
            fprintf(stderr, "partial frames: %d frame\n", rc);
        }

        for(int i = 0; i < frames; i++)
        {
            signal[i] = mic_buffer[i] / (double)(0xffff);
        }

        struct rgb* ptr2 = fb;
        struct rgb* ptr;

        ptr2 += w * (h - 1);
        
        memset(fb, 0, w * h * 3);
        fft(signal, n, inverse);

        if(inverse)
        {
            max = creal(signal[0]);
            min = creal(signal[0]);

            for(int i = 1; i < n; i++)
            {
                double mag = creal(signal[i]);
    
                if(mag < min)
                    min = mag;
                else if(mag > max)
                    max = mag;
            }
        }
        else
        {
            max = cabs(signal[0]);
            min = cabs(signal[0]);

            for(int i = 1; i < n; i++)
            {
                double mag = cabs(signal[i]);
    
                if(mag < min)
                    min = mag;
                else if(mag > max)
                    max = mag;
            }
        }

        range = max - min;

        if(!inverse)
        {
            for(int i = 0; i < n; i++)
            {
                double magnitude = (cabs(signal[i]) - min) / range;
                
                ptr = (ptr2 - w * (int)((h - 1) * magnitude));// magnitude));
                
                ptr->r = 0;
                ptr->g = 255;
                ptr->b = 0;

                ptr2++;
            }
        }
        else
        {
            for(int i = 0; i < n; i++)
            {
                double magnitude = (creal(signal[i]) - min) / range;

                ptr = (ptr2 - w * (int)((h - 1) * magnitude));
                
                ptr->r = 0;
                ptr->g = 255;
                ptr->b = 0;

                ptr2++;
            }
        }

        kitty_update_display(frame, w, h);

        f++;
        //inverse ^= 1;

        usleep(1000000);
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    free(mic_buffer);
    free(fb);

    return 0;
}