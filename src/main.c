#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <math.h>
#include <complex.h>
#include <float.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "fft.h"
#include "draw.h"
#include "microphone.h"

#define WIDTH 256
#define HEIGHT 128
#define NSAMPLES 256 //must be power of 2!
#define DB_NOISE_THRESH 90

bool dofft = false;
bool exit_req = false;

void generate_sin_wave(int16_t* buffer, int samples, double frequency)
{
    double pi = acos(-1);
    double pi2 = 2.0 * pi;
    double coeff = frequency * pi2 / (double)samples;

    double c = 0;

    for(int i = 0; i < samples; i++, c += coeff)
    {
        double val = (sin(c) + 1.0) * 0.5;
        buffer[i] = val * (UINT16_MAX - 1) - INT16_MAX;
    }
}

void get_range_sound(int16_t* buffer, uint32_t n, int16_t* min, int16_t* max)
{
    if(n == 0)
        return;

    *max = buffer[0];
    *min = buffer[0];

    for(uint32_t i = 1; i < n; i++)
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

    *max = fftbuffer[0];
    *min = *max;

    for(uint32_t i = 1; i < n; i++)
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
        min = INT16_MIN;
        max = INT16_MAX;
        
        double range = UINT16_MAX;
        
//        generate_sound(buffer, n);
        
        for(int i = 0; i < n; i++)
        {
            double val = buffer[i];
            double norm = (double)val / (double)max;
            
            //[-1, 1]
            fftbuffer[i] = norm;
        }
    }
    else
    {
        double min, max, range, val;
        int uint16max = 0xffff; 
        double j = 0;
        //the fft produces N/2 data from 0 Hz to Nyquist frequency samplerate / 2
        //in my case 44100 Hz / 2
        //im also using the log scale to get less noise

        //magnitude of signal
        for(int i = 0; i < n / 2; i++)
            fftbuffer[i] = cabs(fftbuffer[i]) / (double)n; //sqrt(Re^2 + Im^2) is the magnitude  

        //converting values to decibel scale
        for(int i = 0; i < n / 2; i++)
        {
            val = fftbuffer[i];
            val = 20.0 * log10(val + 1e-12); 

            if(val < -DB_NOISE_THRESH)
                val = -DB_NOISE_THRESH;

            fftbuffer[i] = val; 
        }

        //scaling to decibel range
        max = 0;
        min = -DB_NOISE_THRESH;
        range = max - min;
        
        for(int i = 0; i < n; i++, j += 0.5)
        {
            int index = j;

            double mag = creal(fftbuffer[index]);
            double norm = (mag - min) / range;

            int m1 = (uint16max * norm);
            int wave_val = fmin(INT16_MAX, m1 - INT16_MAX);

            //[-2^15, 2^15 - 1] is the range of values in the final buffer
            buffer[i] = wave_val;
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

#if __linux__
void show_help()
{
    printf("HELP: \"fft-spectrum --device <alsa-device>\"\n");
    printf("HELP: You can use command: \"arecord -l\" to get the list of microphones.\n");
    printf("HELP: You can use command: \"fft-spectrum --device default\" if you want to select the default microphone.\n");
    printf("EXAMPLE: fft-spectrum --device hw:0,1\n");
}
#endif

int main(int argc, char* argv[])
{
#if __linux__
    if(argc < 2)
    {
        show_help();
        return 1;
    }

    bool found = false;

    for(int i = 0; i < argc - 1; i++)
    {
        if(!strcmp(argv[i], "--device"))
        {
            set_microphone(argv[i + 1]);
            found = true;
            break;
        }
    }

    if(!found)
    {
        show_help();
        return 1;
    }
#endif

    //printf("kitty_init\n");
    kitty_init(WIDTH, HEIGHT);

    // printf("init_microphone\n");
    if(!init_microphone(NSAMPLES))
    {
        printf("Failed to init microphone, exiting...\n");
        return 1;
    }

    // printf("enable_raw_mode\n");
    enable_raw_mode();

    printf("Press F to toggle Fourier Transform visualization, ESC for exit.\r\n");

    int16_t *sound = malloc(NSAMPLES * sizeof(int16_t));
    double complex *fftbuffer = malloc(NSAMPLES * sizeof(double complex));
    int f = 0;
    int16_t max, min;

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
            //to improve visibility
            min = INT16_MIN / 16;
            max = INT16_MAX / 16;

            kitty_draw_sound(f, sound, NSAMPLES, min, max);
        }

        f++;
        //usleep(1000 * 1000);
        usleep(16000);
    }

    stop_microphone();
    kitty_stop();

    free(fftbuffer);
    free(sound);

    return 0;
}
