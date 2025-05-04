#include <stdbool.h>
#include <stdint.h>

bool init_microphone(int samples);
void stop_microphone();
void get_microphone_buffer(int16_t* buffer, int samples);