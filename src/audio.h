#pragma once
#include <cstddef>
#include <cstdint>

void setAudioVolume(uint8_t v);   // 0..255
uint8_t getAudioVolume();

void updateAudioVolumeFromPot();
int16_t applyVolumeToSample(int16_t s);
void applyVolumeToBuffer(int16_t* samples, size_t count);

bool initAudio();
void setAudioPaused(bool paused);

// Write 44.1 kHz stereo signed 16-bit PCM. Returns bytes written, or 0 on failure.
// The call blocks as needed to keep playback paced by the audio device.
size_t writeAudio(const void* data, size_t byteCount);
