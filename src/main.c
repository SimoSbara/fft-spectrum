#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <math.h>
#include <complex.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "fft.h"
#include "draw.h"
#include "microphone.h"

#define WIDTH 256
#define HEIGHT 128
#define NSAMPLES 64

bool dofft = false;
bool exit_req = false;

void generate_sound(int16_t* buffer, int samples)
{
    double pi = acos(-1);
    double pi2 = 2.0 * pi;
    double coeff = pi2 / (double)samples;

    double c = 0;
    double codomain = 1.0 / pi2;

    for(int i = 0; i < samples; i++, c += coeff)
    {
        double val = (sin(c) + cos(2 * c + pi / 3) + sin(5 * c - pi / 6)) * codomain;
        buffer[i] = val * (UINT16_MAX) - INT16_MAX;
    }
}

void get_range_sound(int16_t* buffer, uint32_t n, int16_t* min, int16_t* max)
{
    if(n == 0)
        return;

    *max = buffer[0];
    *min = buffer[0];

    for(int i = 1; i < n; i++)
    {
        int16_t mag = buffer[i];

        if(mag < *min)
            *min = mag;
        else if(mag > *max)
            *max = mag;
    }
}

void get_range_fft(double complex* fftbuffer, uint32_t n, double* min, double* max)
{
    if(n == 0)
        return;

    *max = cabs(fftbuffer[0]);
    *min = *max;

    for(int i = 1; i < n; i++)
    {
        double mag = cabs(fftbuffer[i]);

        if(mag < *min)
            *min = mag;
        else if(mag > *max)
            *max = mag;
    }
}

void convert_fft_buffer(int16_t* buffer, double complex* fftbuffer, int n, bool tofft)
{
    if(tofft)
    {
        int16_t min, max;
        get_range_sound(buffer, n, &min, &max);

        double range = max - min;

        //printf("tofft %d %d %f\n", min, max, range);

        for(int i = 0; i < n; i++)
        {
            int val = buffer[i];

            //[0, 1]
            fftbuffer[i] = (double)(val - min) / (double)range;
        }
    }
    else
    {
        double min, max, range;
        get_range_fft(fftbuffer, n, &min, &max);
        range = max - min;

        double uint16max = UINT16_MAX;

        //printf("!tofft %f %f %f\n", min, max, range);

        for(int i = 0; i < n; i++)
        {
            double mag = cabs(fftbuffer[i]);

            //[-2^16 - 1, 2^16 - 1]

            int16_t val = (uint16max * ((mag - min) / range)) - INT16_MAX;

            //printf("val %d\n", val);

            buffer[i] = val;
        }
    }
}

//implementation by antirez c64-kitty
int kbhit() 
{
    int bytesWaiting;
    ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}

void listen_keyboard_inputs()
{
    int new_input = kbhit();
    
    if(!new_input)
        return;

    char ch = getchar();

    //printf("new input %c\n", ch);

    switch(ch)
    {
        case 'f': dofft ^= 1; break;
        case 27: exit_req = true; break;
    }
}

//implementation by antirez c64-kitty
// Terminal keyboard input handling
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() 
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(int argc, char* argv[])
{
    int f = 0, inverse = 0;
    int16_t max, min;

    //printf("kitty_init\n");
    kitty_init(WIDTH, HEIGHT);

    //printf("init_microphone\n");
    if(!init_microphone(NSAMPLES))
        return 1;

    //printf("init_sound\n");
    //init_sound(NSAMPLES);

    enable_raw_mode();

    int16_t *sound = malloc(NSAMPLES * sizeof(int16_t));
    double complex *fftbuffer = malloc(NSAMPLES * sizeof(double complex));

    while(!exit_req)
    {
        listen_keyboard_inputs();
        get_microphone_buffer(sound, NSAMPLES);

        if(dofft)
        {
            convert_fft_buffer(sound, fftbuffer, NSAMPLES, true);
            fft(fftbuffer, NSAMPLES, false);
            convert_fft_buffer(sound, fftbuffer, NSAMPLES, false);
        }

        get_range_sound(sound, NSAMPLES, &min, &max);

       //min = INT16_MIN;
       //max = INT16_MAX;

        kitty_draw_sound(f, sound, NSAMPLES, min, max);

        f++;
    }

    stop_microphone();
    kitty_stop();

    free(fftbuffer);
    free(sound);

    return 0;
}