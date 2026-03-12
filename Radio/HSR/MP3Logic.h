#ifndef MP3_LOGIC_H
#define MP3_LOGIC_H

#include "HelperFunctions.h"
#include <DFRobotDFPlayerMini.h>

#define DFPLAYER_RX 38
#define DFPLAYER_TX 39
#define MP3_MAX_TRACKS 100
#define MP3_MAX_NAME_LEN 32

extern DFRobotDFPlayerMini dfPlayer;
extern int mp3TrackCount;
extern int mp3CurrentTrack;
extern bool mp3Playing;
extern int mp3ListOffset;    // scroll offset for file list
extern int mp3ListSelected;  // currently highlighted track
extern unsigned long mp3TrackStart;  // when current track started
extern unsigned long mp3TrackLength; // estimated length in ms
extern char mp3TrackNames[MP3_MAX_TRACKS][MP3_MAX_NAME_LEN];

enum MP3Screen {
  MP3_LIST,
  MP3_PLAYING
};

extern MP3Screen mp3Screen;

void initMP3Player();
void drawMP3ListScreen();
void drawMP3PlayScreen();
void handleMP3Buttons();
void enterMP3Mode();

#endif // MP3_LOGIC_H