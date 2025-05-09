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
#define NSAMPLES 256
#define DB_NOISE_THRESH -80

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

void get_range_fft(double* fftbuffer, uint32_t n, double* min, double* max)
{
    if(n == 0)
        return;

    *max = fftbuffer[0];
    *min = *max;

    for(int i = 1; i < n; i++)
    {
        double mag = fftbuffer[i];

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
        double uint16max = UINT16_MAX - 1; 
        double j = 0;
        
        //the fft produces N/2 data from 0 Hz to Nyquist frequency samplerate / 2
        //in my case 44100 Hz / 2
        //im also using the log scale to get less noise

        //magnitudes
        for(int i = 0; i < n / 2; i++)
            fftbuffer[i] = pow(cabs(fftbuffer[i]), 2); //sqrt(Re^2 + Im^2) is the magnitude  

        get_range_fft(fftbuffer, n / 2, &min, &max);
        range = max - min;

        //converting values to decibel
        for(int i = 0; i < n / 2; i++)
        {
            double val = (fftbuffer[i] - min) / range;
            
            val = 20.0 * log10(val + 1e-12);

            if(val < DB_NOISE_THRESH)
                val = DB_NOISE_THRESH;

            fftbuffer[i] = val; 
        }
 
        min = DB_NOISE_THRESH;
        max = 0;
        range = -DB_NOISE_THRESH;      

        for(int i = 0; i < n; i++, j += 0.5)
        {
            double mag = fftbuffer[(int)j];

            //[-2^15, 2^15 - 1] is the range of values in the final buffer
            buffer[i] = (uint16max * ((mag - min) / range)) - INT16_MAX;
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

    printf("init_microphone\n");
    if(!init_microphone(NSAMPLES))
    {
        printf("Failed to init microphone, exiting...\n");
        return 1;
    }

    printf("enable_raw_mode\n");
    enable_raw_mode();

    printf("Press F to toggle Fourier Transform visualization, ESC for exit.\r\n");

    int16_t *sound = malloc(NSAMPLES * sizeof(int16_t));
    double complex *fftbuffer = malloc(NSAMPLES * sizeof(double complex));

    while(!exit_req)
    {
        //printf("listen_keyboard_inputs");
        listen_keyboard_inputs();

        //printf("get_microphone_buffer");
        get_microphone_buffer(sound, NSAMPLES);

        if(dofft)
        {
            convert_fft_buffer(sound, fftbuffer, NSAMPLES, true);
            fft(fftbuffer, NSAMPLES, false);
            convert_fft_buffer(sound, fftbuffer, NSAMPLES, false);

            min = INT16_MIN;
            max = INT16_MAX;
        
            kitty_draw_fft(f, sound, NSAMPLES, min, max);
        }
        else
        {
            min = INT16_MIN / 16;
            max = INT16_MAX / 16;

            kitty_draw_sound(f, sound, NSAMPLES, min, max);
        }

        f++;
        usleep(16000);
    }

    stop_microphone();
    kitty_stop();

    free(fftbuffer);
    free(sound);

    return 0;
}
