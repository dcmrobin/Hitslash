#ifndef RADIO_AUDIO_LOGIC_H
#define RADIO_AUDIO_LOGIC_H

#include "HelperFunctions.h"

#define I2S_BCLK 12
#define I2S_LRC  11
#define I2S_DOUT 13

#define POT_PIN A0

extern Audio audio;

extern const char* stations[];

extern const char* stationNames[];

extern const int stationCount;
extern int currentStation;

extern unsigned long lastReconnect;

extern bool speakerEnabled;
extern int maxVolumeSpeakerOn;
extern int maxVolumeSpeakerOff;
extern int lastMaxVolume;
extern unsigned long lastButtonPress;
extern const unsigned long debounceTime;

void drawSpeakerControlScreen();
void handleVolume();
void initMP3VolumeTask();
void startRadio();
void switchStation(int dir);

#endif // RADIO_AUDIO_LOGIC_H