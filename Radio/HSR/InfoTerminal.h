#ifndef INFO_TERMINAL_H
#define INFO_TERMINAL_H

#include "HelperFunctions.h"

// ── Limits ────────────────────────────────────────────────────────────────────
#define IT_MAX_QUERY      100   // max chars the user can type
#define IT_MAX_RESULT_LEN 10000 // result display buffer
#define IT_MAX_OPTIONS    20   // max lookup options shown in menu

// ── Keyboard rows ─────────────────────────────────────────────────────────────
// Row 0: letters
// Row 1: numbers + symbols
// Row 2: actions (SPACE, BKSP, ENTER, EXIT)

// ── Query-type enum ───────────────────────────────────────────────────────────
enum InfoQueryType {
  IQ_WIKIPEDIA = 0,
  IQ_DICTIONARY,
  IQ_WEATHER,
  IQ_SUNRISE,
  IQ_POSTCODE,
  IQ_COUNTRY,
  IQ_IP_LOOKUP,
  IQ_DOMAIN,
  IQ_ISBN,
  IQ_BARCODE,
  IQ_CURRENCY,
  IQ_CRYPTO,
  IQ_VIN,
  IQ_FLIGHT,
  IQ_HISTORY_TODAY,
  IQ_RANDOM_FACT,
  IQ_LEGO_SET_INFO,
  IQ_COUNT  // always last
};

// Info Terminal sub-screen
enum InfoScreen {
  IT_KEYBOARD,
  IT_MENU,
  IT_RESULT,
  IT_HELP
};

extern char itQuery[IT_MAX_QUERY + 1];
extern int  itQueryLen;
extern InfoScreen itScreen;
extern int  itMenuSelected;
extern int  itMenuCount;
extern InfoQueryType itMenuOptions[IT_MAX_OPTIONS];
extern char itResult[IT_MAX_RESULT_LEN];
extern int  itResultScrollOffset;
extern int  itHelpScrollOffset;

// Keyboard state
extern int kbRow;       // 0=letters 1=numbers/symbols 2=actions
extern int kbCol;       // index within current row

void enterInfoTerminal();
void handleInfoTerminalButtons();
void drawInfoKeyboard();
void drawInfoMenu();
void drawInfoResult();
void drawInfoHelp();

void buildInfoMenu();
void runInfoLookup(InfoQueryType qt);

// Individual fetchers
void fetchWikipedia();
void fetchDictionary();
void fetchWeather();
void fetchSunrise();
void fetchPostcode();
void fetchCountry();
void fetchIPLookup();
void fetchDomain();
void fetchISBN();
void fetchBarcode();
void fetchCurrency();
void fetchCrypto();
void fetchVIN();
void fetchFlight();
void fetchHistoryToday();
void fetchRandomFact();
void fetchLegoSetInfo();

// Helpers
bool itLooksLikeIP(const char* s);
bool itLooksLikeVIN(const char* s);
bool itLooksLikeISBN(const char* s);
bool itLooksLikeBarcode(const char* s);
bool itLooksLikeFlight(const char* s);
bool itLooksLikeCurrency(const char* s);
bool itLooksLikeCrypto(const char* s);
bool itLooksLikeUKPostcode(const char* s);
bool itLooksLikeCountry(const char* s);
bool itLooksLikeDomain(const char* s);
const char* queryTypeName(InfoQueryType qt);

void itAppendResult(const char* text);
void itSetResult(const char* text);

#endif // INFO_TERMINAL_H