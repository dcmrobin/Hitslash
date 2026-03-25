#ifndef WIFI_LOGIC_H
#define WIFI_LOGIC_H


#include "HelperFunctions.h"

#define LTE_MOSFET_PIN 15
//#define SPEAKER_MOSFET_PIN 16
#define MODEM_SSID "hitslash-router"
#define MODEM_PASSWORD "hitslashradio"

extern bool modemPoweredOn;

extern WebServer server;
extern DNSServer dnsServer;
extern Preferences preferences;

extern unsigned long lastStatusUpdate;
extern const char* AP_SSID;
extern IPAddress apIP;
extern IPAddress netMask;

// Saved networks list
extern String savedSSIDs[10];
extern String savedPasswords[10];
extern int savedCount;
extern int selectedNetworkIndex;

// HTML page for captive portal
extern const char* htmlPage;

void loadSavedNetworks();
void saveNetwork(String ssid, String password);
void clearAllNetworks();
void deleteNetwork(int index);
bool tryConnect(const char* ssid, const char* password);
bool connectToSavedNetworks();
bool connectToModem();
void handleRoot();
void handleSave();
void handleManage();
void handleClear();
void handleNotFound();
void enterSetupMode();

#endif // WIFI_LOGIC_H