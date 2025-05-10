#include "microphone.h"

#if __linux__
#include <alsa/asoundlib.h>
#include <math.h>
#include <stdio.h>

#define PCM_DEVICE "default"
#define MAX_SOUND_CARDS 8
#define MAX_MICROPHONES_PER_CARD 8

char pcm_device[64];

snd_pcm_t *pcm_handle;
snd_pcm_hw_params_t *params;
snd_pcm_uframes_t frames;
unsigned int sample_rate = 44100;
int dir;

//set manually
void set_microphone(char* dev)
{
    strcpy(pcm_device, dev);
}

//open microphone
bool init_microphone(int samples)
{
    int rc = snd_pcm_open(&pcm_handle, pcm_device, SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) 
    {
        fprintf(stderr, "Error opening device PCM: %s\n", snd_strerror(rc));
        return false;
    }

    snd_pcm_hw_params_malloc(&params);

    frames = samples;

    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, params, 1);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &sample_rate, &dir);
    snd_pcm_hw_params_set_period_size(pcm_handle, params, frames, dir);

    // Parameters
    rc = snd_pcm_hw_params(pcm_handle, params);
    if (rc < 0)
    {
        fprintf(stderr, "Error setting parameters: %s\n", snd_strerror(rc));
        return false;
    }

    return true;
}

void stop_microphone()
{
    snd_pcm_drop(pcm_handle);
    snd_pcm_close(pcm_handle);
}

void get_microphone_buffer(int16_t* buffer, int samples)
{
    //printf("get_microphone_buffer()\n");

    int rc = snd_pcm_readi(pcm_handle, buffer, fmin(samples, frames));

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
}

#elif defined(__APPLE__) && defined(__MACH__)
#include <pthread.h>
#include <AudioToolbox/AudioToolbox.h>
#define kBufferCount 3

AudioQueueRef queue;
OSStatus status;

pthread_mutex_t mic_mutex;
int16_t *mic_buffer = 0;
bool acquired = false;

int kBufferSize;
int frames;

void HandleInputBuffer(
    void *inUserData,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumPackets,
    const AudioStreamPacketDescription *inPacketDesc)
{
    int16_t* new_buffer = inBuffer->mAudioData;

    //printf("%d\n", samples);

    pthread_mutex_lock(&mic_mutex);
    memcpy(mic_buffer, new_buffer, kBufferSize);
    acquired = true;
    pthread_mutex_unlock(&mic_mutex);

    AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
}

bool init_microphone(int samples)
{
    AudioStreamBasicDescription format;
    memset(&format, 0, sizeof(format));

    kBufferSize = samples * sizeof(int16_t);
    frames = samples;

    format.mSampleRate = 44100;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format.mFramesPerPacket = 1;
    format.mChannelsPerFrame = 1;
    format.mBitsPerChannel = 16;
    format.mBytesPerPacket = 2;
    format.mBytesPerFrame = 2;

    printf("AudioQueueNewInput\n");

    status = AudioQueueNewInput(
        &format,
        HandleInputBuffer,
        0,
        0,
        kCFRunLoopCommonModes,
        0,
        &queue
    );

    if (status != noErr) {
        fprintf(stderr, "Error creation AudioQueue: %d\n", status);
        return false;
    }

    printf("kBufferCount\n");

    // Allocate buffer
    for (int i = 0; i < kBufferCount; ++i) {
        AudioQueueBufferRef buffer;
        AudioQueueAllocateBuffer(queue, kBufferSize, &buffer);
        AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
    }

    printf("AudioQueueStart: %d\n", kBufferSize);
    AudioQueueStart(queue, NULL);

    printf("malloc(kBufferSize)\n");
    mic_buffer = malloc(kBufferSize);

    return true;
}

void stop_microphone()
{
    AudioQueueStop(queue, true);
    AudioQueueDispose(queue, true);

    if(mic_buffer)
        free(mic_buffer);
}

void get_microphone_buffer(int16_t* buffer, int samples)
{    
    pthread_mutex_lock(&mic_mutex);

    if(acquired)
    {
        memcpy(buffer, mic_buffer, fmin(frames, samples) * sizeof(int16_t));
        acquired = false;
    }

    pthread_mutex_unlock(&mic_mutex);
}

#endif