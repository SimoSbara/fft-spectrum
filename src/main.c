#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <complex.h>
#include <pthread.h>

#include "fft.h"
#include "draw.h"

#define WIDTH 512
#define HEIGHT 128

double complex *sound;

void init_sound(int samples)
{
    sound = malloc(samples * sizeof(double complex));

    double pi = acos(-1);
    double coeff = 2.0 * pi / (double)samples;
    double c = 0;

    for(int i = 0; i < samples; i++, c += coeff)
    {
        sound[i] = sin(c) + cos(2 * c + pi / 3) + sin(5 * c - pi / 6);
    }
}

void get_range_sound(double complex* buffer, int n, double* min, double* max)
{
    if(n <= 0)
        return;

    *max = cabs(sound[0]);
    *min = cabs(sound[0]);

    for(int i = 1; i < n; i++)
    {
        double mag = cabs(sound[i]);

        if(mag < *min)
            *min = mag;
        else if(mag > *max)
            *max = mag;
    }
}

#if __linux__
#include <alsa/asoundlib.h>

#define PCM_DEVICE "default"

snd_pcm_t *pcm_handle;
snd_pcm_hw_params_t *params;
snd_pcm_uframes_t SAMPLES = WIDTH;
unsigned int sample_rate = 44100;
int dir;

uint16_t *mic_buffer;
int size;

//open microphone
bool init_microphone()
{
    int rc = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) 
    {
        fprintf(stderr, "Error opening device PCM: %s\n", snd_strerror(rc));
        return false;
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, params, 1);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &sample_rate, &dir);

    // Parameters
    rc = snd_pcm_hw_params(pcm_handle, params);
    if (rc < 0)
    {
        fprintf(stderr, "Error setting parameters: %s\n", snd_strerror(rc));
        return false;
    }

    snd_pcm_hw_params_get_period_size(params, &SAMPLES, &dir);
    size = SAMPLES * 2; //16 bit
    mic_buffer = (uint16_t*)malloc(size);

    return true;
}

void stop_microphone()
{
    if(mic_buffer)
    {
        snd_pcm_drain(pcm_handle);
        snd_pcm_close(pcm_handle);
        free(mic_buffer);
    }
}

void get_microphone_buffer(double complex* buffer, int samples)
{
    int rc = snd_pcm_readi(mic_buffer, buffer, samples);

    if (rc < 0)
    {
        switch(rc)
        {
            case -EPIPE:
                fprintf(stderr, "Overflow!\n");
                snd_pcm_prepare(pcm_handle);
            break;
            default:
                fprintf(stderr, "Error microphone: %s\n", snd_strerror(rc)); 
            break;
        }
    }
    else if (rc != (int)samples) 
        fprintf(stderr, "Partial frames: %d frame\n", rc);

    for(int i = 0; i < SAMPLES; i++)
       buffer[i] = mic_buffer[i] / (double)(0xffff);
}

#elif defined(__APPLE__) && defined(__MACH__)
#include <AudioToolbox/AudioToolbox.h>
#define kBufferCount 3
#define kBufferSize  WIDTH
#define SAMPLES kBufferSize

AudioQueueRef queue;
OSStatus status;

pthread_mutex_t mic_mutex;

uint16_t* tmp_buffer;
bool acquired = false;

void HandleInputBuffer(
    void *inUserData,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumPackets,
    const AudioStreamPacketDescription *inPacketDesc)
{
    uint16_t* new_buffer = inBuffer->mAudioData;

    pthread_mutex_lock(&mic_mutex);

    memcpy(tmp_buffer, new_buffer, SAMPLES * sizeof(uint16_t));
    acquired = true;

    pthread_mutex_unlock(&mic_mutex);

    AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
}

bool init_microphone()
{
    AudioStreamBasicDescription format;
    memset(&format, 0, sizeof(format));

    format.mSampleRate = 44100;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format.mFramesPerPacket = 1;
    format.mChannelsPerFrame = 1;
    format.mBitsPerChannel = 16;
    format.mBytesPerPacket = 2;
    format.mBytesPerFrame = 2;

    status = AudioQueueNewInput(
        &format,
        HandleInputBuffer,
        NULL,
        NULL,
        kCFRunLoopCommonModes,
        0,
        &queue
    );

    if (status != noErr) {
        fprintf(stderr, "Error creation AudioQueue: %d\n", status);
        return false;
    }

    // Allocate buffer
    for (int i = 0; i < kBufferCount; ++i) {
        AudioQueueBufferRef buffer;
        AudioQueueAllocateBuffer(queue, kBufferSize, &buffer);
        AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
    }

    AudioQueueStart(queue, NULL);

    tmp_buffer = (uint16_t*)malloc(SAMPLES * sizeof(uint16_t));

    return true;
}

void stop_microphone()
{
    if(status != noErr)
    {
        AudioQueueStop(queue, true);
        AudioQueueDispose(queue, true);
        free(tmp_buffer);
    }
}

void get_microphone_buffer(double complex* buffer, int samples)
{    
    pthread_mutex_lock(&mic_mutex);

    if(acquired)
    {
        for(int i = 0; i < SAMPLES; i++)
            buffer[i] = tmp_buffer[i];

        acquired = false;
    }

    pthread_mutex_unlock(&mic_mutex);
}

#endif

int main(int argc, char* argv[])
{
    printf("kitty_init\n");
    kitty_init(WIDTH, HEIGHT);

    printf("init_microphone\n");
    if(!init_microphone())
        return 1;

    printf("init_sound\n");
    init_sound(SAMPLES);

    int f = 0, inverse = 0;
    double max, min;

    while(f < 20)
    {
        get_microphone_buffer(sound, SAMPLES);
        get_range_sound(sound, SAMPLES, &min, &max);

        //fft(sound, SAMPLES, false);

        kitty_draw_sound(0, sound, SAMPLES, min, max);

        f++;
        inverse ^= 1;

        usleep(1000000);
    }

    stop_microphone();
    kitty_stop();

    return 0;
}