#pragma once
#include <cstddef>

void setAudioVolume(uint8_t v);   // 0..255
uint8_t getAudioVolume();

void updateAudioVolumeFromPot();
int16_t applyVolumeToSample(int16_t s);
void applyVolumeToBuffer(int16_t* samples, size_t count);

bool initAudio();
