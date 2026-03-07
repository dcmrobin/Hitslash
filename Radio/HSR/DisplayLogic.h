#ifndef DISPLAY_LOGIC_H
#define DISPLAY_LOGIC_H

#include "HelperFunctions.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128

#define OLED_DC     9
#define OLED_CS     10
#define OLED_RESET  8

extern Adafruit_SH1107 display;

enum DisplayMode {
  DISPLAY_STATION,
  DISPLAY_WIFI_INFO,
  DISPLAY_SPEAKER_CTRL,
  DISPLAY_NETWORK_LIST,
  DISPLAY_MAJOR_ORDER,
  DISPLAY_NEWS
};

extern DisplayMode currentDisplay;

#define MAX_LINES 100
extern String lines[MAX_LINES];
extern int totalLines;
extern int scrollOffset;
extern int lineHeight;
extern int maxVisibleLines;

void drawProgressBar(int x, int y, int width, int height, float percent);
void drawWrappedText(String text, int x, int y, int maxWidth, int lineHeight);
void drawSetupScreen();
void drawRadioScreen();
void drawWifiInfoScreen();
void drawNetworkListScreen();
void drawBatteryIcon(int x, int y);
void drawHelldiversMajorOrder();
void drawHelldiversNews();
int drawWrappedText(String text, int x, int y, int maxWidth);
void buildSetupText();
void buildNetworkListText();
void buildConnectingText(const char* message);

#endif // DISPLAY_LOGIC_H