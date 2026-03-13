// MP3Logic.h
#ifndef MP3_LOGIC_H
#define MP3_LOGIC_H

#include "HelperFunctions.h"

#define DFPLAYER_RX 38
#define DFPLAYER_TX 39
#define MP3_MAX_TRACKS 100
#define MP3_MAX_NAME_LEN 32

extern int mp3TrackCount;
extern int mp3CurrentTrack;
extern bool mp3Playing;
extern int mp3ListOffset;
extern int mp3ListSelected;
extern unsigned long mp3TrackStart;
extern unsigned long mp3PausedAt;
extern unsigned long mp3TotalPaused;

enum MP3Screen {
  MP3_LIST,
  MP3_PLAYING
};

extern MP3Screen mp3Screen;
extern char mp3TrackNames[MP3_MAX_TRACKS][MP3_MAX_NAME_LEN];

// Raw UART commands
void mp3SendCommand(byte command, int param = 0);
void mp3Play(int track);
void mp3Pause();
void mp3Resume();
void mp3Stop();
void mp3SetVolume(int vol);
void mp3Next();
void mp3Prev();
int  mp3GetTrackCount();

// Mode functions
void initMP3Player();
void drawMP3ListScreen();
void drawMP3PlayScreen();
void handleMP3Buttons();
void enterMP3Mode();

#endif // MP3_LOGIC_H