#include "InfoTerminal.h"

// ── State ─────────────────────────────────────────────────────────────────────
char itQuery[IT_MAX_QUERY + 1] = "";
int  itQueryLen = 0;
InfoScreen itScreen = IT_KEYBOARD;
int  itMenuSelected = 0;
int  itMenuCount = 0;
InfoQueryType itMenuOptions[IT_MAX_OPTIONS];
char itResult[IT_MAX_RESULT_LEN] = "";
int  itResultScrollOffset = 0;
int  itHelpScrollOffset = 0;

int kbRow = 0;
int kbCol = 0;

// ── Keyboard layout ───────────────────────────────────────────────────────────
const char* kbLetters  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char* kbNumSym   = "0123456789.,'-@#/()!?&+:*+=";
// Row 2 actions rendered separately: SPACE  BKSP  ENTER  EXIT
const char* kbActionLabels[] = { "SPC", "BKSP", "ENTR", "EXIT" };
const int   kbActionCount    = 4;

int kbRowLen(int row) {
  if (row == 0) return strlen(kbLetters);
  if (row == 1) return strlen(kbNumSym);
  return kbActionCount;
}

// ── Entry point ───────────────────────────────────────────────────────────────
void enterInfoTerminal() {
  itQueryLen = 0;
  itQuery[0] = '\0';
  itScreen   = IT_KEYBOARD;
  kbRow = 0;
  kbCol = 0;
  itResultScrollOffset = 0;
  itHelpScrollOffset = 0;
  drawInfoKeyboard();
}

// ── Drawing: keyboard ─────────────────────────────────────────────────────────
void drawInfoKeyboard() {
  display.clearDisplay();
  display.setTextSize(1);

  // ── typed text area (top 22 px, wraps) ──
  display.drawRect(0, 0, 128, 22, SH110X_WHITE);
  display.setCursor(2, 2);
  // Show last 18 chars if too long to fit
  int start = 0;
  if (itQueryLen > 18) start = itQueryLen - 18;
  for (int i = start; i < itQueryLen; i++) {
    display.print(itQuery[i]);
  }
  // blinking cursor placeholder
  display.print("_");

  // ── row 0: letters (two sub-rows of 13) ──
  // We show a window of 13 chars centred on kbCol for row 0/1
  // and all 4 actions for row 2.
  // Active row is highlighted.

  int y = 26;

  // Letters row  (A-M top, N-Z bottom — but we scroll to show cursor)
  {
    bool active = (kbRow == 0);
    int len = strlen(kbLetters);
    // Show a window of 21 chars centred around kbCol
    int winStart = kbCol - 10;
    if (winStart < 0) winStart = 0;
    if (winStart + 21 > len) winStart = len - 21;
    if (winStart < 0) winStart = 0;

    for (int i = 0; i < 21 && (winStart + i) < len; i++) {
      int idx = winStart + i;
      int cx  = i * 6;
      if (active && idx == kbCol) {
        display.fillRect(cx, y, 6, 9, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
      }
      display.setCursor(cx, y + 1);
      display.print(kbLetters[idx]);
      display.setTextColor(SH110X_WHITE);
    }
  }
  y += 12;

  // Numbers/symbols row
  {
    bool active = (kbRow == 1);
    int len = strlen(kbNumSym);
    int winStart = kbCol - 10;
    if (winStart < 0) winStart = 0;
    if (winStart + 21 > len) winStart = len - 21;
    if (winStart < 0) winStart = 0;

    for (int i = 0; i < 21 && (winStart + i) < len; i++) {
      int idx = winStart + i;
      int cx  = i * 6;
      if (active && idx == kbCol) {
        display.fillRect(cx, y, 6, 9, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
      }
      display.setCursor(cx, y + 1);
      display.print(kbNumSym[idx]);
      display.setTextColor(SH110X_WHITE);
    }
  }
  y += 12;

  // Actions row
  {
    bool active = (kbRow == 2);
    // 4 buttons spread across 128px = 32px each
    for (int i = 0; i < kbActionCount; i++) {
      int cx = i * 31;
      bool sel = (active && kbCol == i);
      if (sel) {
        display.fillRect(cx, y, 30, 10, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
      } else {
        display.drawRect(cx, y, 30, 10, SH110X_WHITE);
      }
      display.setCursor(cx + 2, y + 1);
      display.print(kbActionLabels[i]);
      display.setTextColor(SH110X_WHITE);
    }
  }
  y += 14;

  // ── hint ──
  display.setCursor(0, y);
  drawWrappedText("L/R:move U/D:row REF:type           HOLD REF:help       INFO TERMINAL", 0, y, 128, 11);

  // battery
  drawBatteryIcon(0, display.height() - 15);
  display.display();
}

// ── Drawing: menu ─────────────────────────────────────────────────────────────
void drawInfoMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("LOOKUP TYPE:");
  display.println(itQuery);
  display.drawLine(0, 20, 128, 20, SH110X_WHITE);

  int maxVisible = 9;
  int scrollStart = 0;
  if (itMenuSelected >= maxVisible) scrollStart = itMenuSelected - maxVisible + 1;

  for (int i = 0; i < maxVisible; i++) {
    int idx = scrollStart + i;
    if (idx >= itMenuCount) break;
    int y = 23 + i * 11;
    bool sel = (idx == itMenuSelected);
    if (sel) {
      display.fillRect(0, y - 1, 128, 11, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    }
    display.setCursor(2, y);
    display.print(queryTypeName(itMenuOptions[idx]));
    display.setTextColor(SH110X_WHITE);
  }
  display.display();
}

// ── Drawing: result ───────────────────────────────────────────────────────────
void drawInfoResult() {
  display.setTextSize(1);

  // Measure true content height with a dry run from y=0 (no scroll offset)
  // drawWrappedText returns the Y position of the line after the last one
  int contentBottom = drawWrappedText(String(itResult), 0, 0, 128);
  display.clearDisplay();

  // Usable screen height minus bottom hint bar
  const int screenH = 118;

  // Clamp scroll: 0 = top, minScroll = furthest down we can go
  int minScroll = -(contentBottom - screenH);
  if (minScroll > 0) minScroll = 0; // content fits on screen, no scroll needed
  itResultScrollOffset = constrain(itResultScrollOffset, minScroll, 0);

  // Now draw for real at the clamped scroll offset
  drawWrappedText(String(itResult) + "\nREF to exit", 0, itResultScrollOffset, 128);

  display.display();
}


// ── Drawing: help ─────────────────────────────────────────────────────────────

static const char IT_HELP_TEXT[] =
  "INFO TERMINAL HELP\n"
  "==================\n\n"
  "NAVIGATION\n"
  "L/R: scroll keyboard row\n"
  "U/D: switch row\n"
  "REFRESH: select key\n"
  "Hold REFRESH: this help\n\n"
  "LOOKUP TYPES\n"
  "------------\n\n"
  "WIKIPEDIA\n"
  "Any search term.\n"
  "e.g. BLACK HOLE\n\n"
  "DICTIONARY\n"
  "Single word.\n"
  "e.g. EPHEMERAL\n\n"
  "WEATHER\n"
  "City, postcode, country.\n"
  "e.g. MANCHESTER, SK13\n\n"
  "SUNRISE/SUNSET\n"
  "City or place name.\n"
  "e.g. GLOSSOP\n\n"
  "UK POSTCODE\n"
  "UK postcode, space optional.\n"
  "e.g. SK13 8AR or SK138AR\n\n"
  "COUNTRY INFO\n"
  "Full country name.\n"
  "e.g. FRANCE, SOUTH KOREA\n\n"
  "IP LOOKUP\n"
  "Dotted IP address.\n"
  "e.g. 8.8.8.8\n"
  "Type ME for your own IP.\n\n"
  "DOMAIN LOOKUP\n"
  "Domain with dot.\n"
  "e.g. GOOGLE.COM\n\n"
  "ISBN / BOOK\n"
  "10 or 13 digits, dashes ok.\n"
  "e.g. 9780306406157\n\n"
  "BARCODE / PRODUCT\n"
  "8 to 14 digits.\n"
  "e.g. 5000112546415\n\n"
  "CURRENCY\n"
  "FROM TO or AMOUNT FROM TO.\n"
  "e.g. USD GBP\n"
  "e.g. 100 USD GBP\n\n"
  "CRYPTO\n"
  "Coin name or ticker.\n"
  "e.g. BITCOIN, BTC, ETH\n\n"
  "VIN DECODER\n"
  "17-char vehicle ID.\n"
  "No I, O or Q allowed.\n\n"
  "FLIGHT\n"
  "2 letters + 1-4 digits.\n"
  "e.g. BA123, EZY1234\n"
  "Needs Aviation API key.\n\n"
  "THIS DAY IN HISTORY\n"
  "Input ignored. Shows\n"
  "notable events from\n"
  "today in history\n"
  "via Wikipedia.\n\n"
  "RANDOM FACT\n"
  "Input ignored. Returns\n"
  "a random useless fact.\n\n"
  "LEGO SET INFO\n"
  "Input a set number\n"
  "to get data about it.\n\n"
  "ESV BIBLE\n"
  "Book chapter:verse ref.\n"
  "e.g. JOHN 3:16\n"
  "e.g. PSALM 23:1-6\n\n"
  "ASK AI\n"
  "Any short question.\n"
  "Uses GPT-4o mini.\n"
  "e.g. WHAT IS A QUASAR\n\n"
  "Hold REFRESH to close.";

void drawInfoHelp() {
  display.setTextSize(1);
  int contentBottom = drawWrappedText(String(IT_HELP_TEXT), 0, 0, 128);
  display.clearDisplay();

  const int screenH = 118;
  int minScroll = -(contentBottom - screenH);
  if (minScroll > 0) minScroll = 0;
  itHelpScrollOffset = constrain(itHelpScrollOffset, minScroll, 0);

  drawWrappedText(String(IT_HELP_TEXT), 0, itHelpScrollOffset, 128);
  display.display();
}

// ── Button handler ────────────────────────────────────────────────────────────
void handleInfoTerminalButtons() {
  // ── RESULT screen ──────────────────────────────────────────────────────────
  if (itScreen == IT_RESULT) {
    if (buttons[BTN_IDX_UP].pressed) {
      itResultScrollOffset += 10;
      if (itResultScrollOffset > 0) itResultScrollOffset = 0;
      drawInfoResult();
    }
    if (buttons[BTN_IDX_DOWN].pressed) {
      itResultScrollOffset -= 10;
      drawInfoResult();
    }
    // Hold select to exit back to keyboard
    if (buttons[BTN_IDX_REFRESH].pressed) {
      itScreen = IT_KEYBOARD;
      itQueryLen = 0;
      itQuery[0] = '\0';
      itResultScrollOffset = 0;
      kbRow = 0; kbCol = 0;
      drawInfoKeyboard();
    }
    return;
  }

  // ── MENU screen ────────────────────────────────────────────────────────────
  if (itScreen == IT_MENU) {
    if (buttons[BTN_IDX_UP].pressed) {
      itMenuSelected--;
      if (itMenuSelected < 0) itMenuSelected = itMenuCount - 1;
      drawInfoMenu();
    }
    if (buttons[BTN_IDX_DOWN].pressed) {
      itMenuSelected++;
      if (itMenuSelected >= itMenuCount) itMenuSelected = 0;
      drawInfoMenu();
    }
    if (buttons[BTN_IDX_REFRESH].pressed) {
      // confirm selection → run lookup
      runInfoLookup(itMenuOptions[itMenuSelected]);
    }
    // LEFT = back to keyboard without clearing query
    if (buttons[BTN_IDX_LEFT].pressed) {
      itScreen = IT_KEYBOARD;
      drawInfoKeyboard();
    }
    return;
  }


  // ── HELP screen ──────────────────────────────────────────────────────────────
  if (itScreen == IT_HELP) {
    if (buttons[BTN_IDX_UP].pressed) {
      itHelpScrollOffset += 10;
      if (itHelpScrollOffset > 0) itHelpScrollOffset = 0;
      drawInfoHelp();
    }
    if (buttons[BTN_IDX_DOWN].pressed) {
      itHelpScrollOffset -= 10;
      drawInfoHelp();
    }
    if (buttons[BTN_IDX_REFRESH].pressed) {
      itScreen = IT_KEYBOARD;
      drawInfoKeyboard();
      return;
    }
    return;
  }

  // ── KEYBOARD screen ────────────────────────────────────────────────────────
  bool redraw = false;

  // LEFT / RIGHT — scroll within row (with wrap)
  if (buttons[BTN_IDX_LEFT].pressed) {
    kbCol--;
    int len = kbRowLen(kbRow);
    if (kbCol < 0) kbCol = len - 1;
    redraw = true;
  }
  if (buttons[BTN_IDX_RIGHT].pressed) {
    kbCol++;
    int len = kbRowLen(kbRow);
    if (kbCol >= len) kbCol = 0;
    redraw = true;
  }

  // UP / DOWN — switch row, clamp col
  if (buttons[BTN_IDX_UP].pressed) {
    kbRow--;
    if (kbRow < 0) kbRow = 2;
    if (kbCol >= kbRowLen(kbRow)) kbCol = kbRowLen(kbRow) - 1;
    redraw = true;
  }
  if (buttons[BTN_IDX_DOWN].pressed) {
    kbRow++;
    if (kbRow > 2) kbRow = 0;
    if (kbCol >= kbRowLen(kbRow)) kbCol = kbRowLen(kbRow) - 1;
    redraw = true;
  }

  // REFRESH hold on keyboard → show help
  if (buttons[BTN_IDX_REFRESH].held && millis() - buttons[BTN_IDX_REFRESH].pressTime > 1500) {
      itScreen = IT_HELP;
      itHelpScrollOffset = 0;
      drawInfoHelp();
      return;
  }

  // SELECT — type the character or trigger action
  if (buttons[BTN_IDX_REFRESH].pressed) {
    if (kbRow == 0) {
      // letter
      if (itQueryLen < IT_MAX_QUERY) {
        itQuery[itQueryLen++] = kbLetters[kbCol];
        itQuery[itQueryLen]   = '\0';
      }
      redraw = true;
    } else if (kbRow == 1) {
      // number / symbol
      if (itQueryLen < IT_MAX_QUERY) {
        itQuery[itQueryLen++] = kbNumSym[kbCol];
        itQuery[itQueryLen]   = '\0';
      }
      redraw = true;
    } else {
      // action
      switch (kbCol) {
        case 0: // SPACE
          if (itQueryLen < IT_MAX_QUERY) {
            itQuery[itQueryLen++] = ' ';
            itQuery[itQueryLen]   = '\0';
          }
          redraw = true;
          break;
        case 1: // BACKSPACE
          if (itQueryLen > 0) {
            itQuery[--itQueryLen] = '\0';
          }
          redraw = true;
          break;
        case 2: // ENTER — go to menu
          if (itQueryLen > 0) {
            buildInfoMenu();
            itScreen = IT_MENU;
            drawInfoMenu();
          }
          break;
        case 3: // EXIT — go back to radio
          currentStation = 0;
          currentMode    = MODE_RADIO;
          currentDisplay = DISPLAY_STATION;
          switchStation(0);
          //drawRadioScreen();
          //audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
          //audio.connecttohost(stations[currentStation]);
          break;
      }
    }
  }

  if (redraw) drawInfoKeyboard();
}

// ── Menu builder — ranks options by relevance ─────────────────────────────────
void buildInfoMenu() {
  itMenuCount    = 0;
  itMenuSelected = 0;

  // Scored list — higher = more relevant
  struct { InfoQueryType qt; int score; } ranked[IQ_COUNT];
  for (int i = 0; i < IQ_COUNT; i++) {
    ranked[i].qt    = (InfoQueryType)i;
    ranked[i].score = 0;
  }

  const char* q = itQuery;

  if (itLooksLikeVIN(q))        ranked[IQ_VIN].score       += 100;
  if (itLooksLikeIP(q))         ranked[IQ_IP_LOOKUP].score += 100;
  if (itLooksLikeISBN(q))       ranked[IQ_ISBN].score      += 90;
  if (itLooksLikeBarcode(q))    ranked[IQ_BARCODE].score   += 80;
  if (itLooksLikeFlight(q))     ranked[IQ_FLIGHT].score    += 90;
  if (itLooksLikeCurrency(q))   ranked[IQ_CURRENCY].score  += 90;
  if (itLooksLikeCrypto(q))     ranked[IQ_CRYPTO].score    += 80;
  if (itLooksLikeUKPostcode(q)) {
    ranked[IQ_POSTCODE].score  += 100;
    ranked[IQ_WEATHER].score   += 60;
    ranked[IQ_SUNRISE].score   += 60;
  }
  if (itLooksLikeCountry(q)) {
    ranked[IQ_COUNTRY].score   += 90;
    ranked[IQ_WEATHER].score   += 50;
    ranked[IQ_SUNRISE].score   += 40;
  }
  if (itLooksLikeDomain(q))     ranked[IQ_DOMAIN].score    += 90;

  // Baseline scores for universally useful options
  ranked[IQ_WIKIPEDIA].score   += 10;
  ranked[IQ_DICTIONARY].score  += 8;
  ranked[IQ_WEATHER].score     += 5;
  ranked[IQ_HISTORY_TODAY].score += 3;
  ranked[IQ_RANDOM_FACT].score  += 1;
  ranked[IQ_ESV_BIBLE].score   += 2;
  ranked[IQ_OPENAI_ASK].score  += 4;

  // Bubble sort descending
  for (int i = 0; i < IQ_COUNT - 1; i++) {
    for (int j = 0; j < IQ_COUNT - 1 - i; j++) {
      if (ranked[j].score < ranked[j+1].score) {
        auto tmp = ranked[j]; ranked[j] = ranked[j+1]; ranked[j+1] = tmp;
      }
    }
  }

  // Add all (limit to IT_MAX_OPTIONS)
  for (int i = 0; i < IQ_COUNT && itMenuCount < IT_MAX_OPTIONS; i++) {
    itMenuOptions[itMenuCount++] = ranked[i].qt;
  }
}

// ── Dispatcher ────────────────────────────────────────────────────────────────
void runInfoLookup(InfoQueryType qt) {
  // Show loading screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.println("Fetching data...");
  display.setCursor(0, 45);
  display.println(queryTypeName(qt));
  display.display();

  itSetResult("");
  itResultScrollOffset = 0;

  switch (qt) {
    case IQ_WIKIPEDIA:    fetchWikipedia();    break;
    case IQ_DICTIONARY:   fetchDictionary();   break;
    case IQ_WEATHER:      fetchWeather();      break;
    case IQ_SUNRISE:      fetchSunrise();      break;
    case IQ_POSTCODE:     fetchPostcode();     break;
    case IQ_COUNTRY:      fetchCountry();      break;
    case IQ_IP_LOOKUP:    fetchIPLookup();     break;
    case IQ_DOMAIN:       fetchDomain();       break;
    case IQ_ISBN:         fetchISBN();         break;
    case IQ_BARCODE:      fetchBarcode();      break;
    case IQ_CURRENCY:     fetchCurrency();     break;
    case IQ_CRYPTO:       fetchCrypto();       break;
    case IQ_VIN:          fetchVIN();          break;
    case IQ_FLIGHT:       fetchFlight();       break;
    case IQ_HISTORY_TODAY: fetchHistoryToday(); break;
    case IQ_RANDOM_FACT:  fetchRandomFact();   break;
    case IQ_LEGO_SET_INFO: fetchLegoSetInfo(); break;
    case IQ_ESV_BIBLE:     fetchESVBible();    break;
    case IQ_OPENAI_ASK:    fetchOpenAIAsk();   break;
    default: itSetResult("Not implemented."); break;
  }

  itScreen = IT_RESULT;
  drawInfoResult();
}

// ═══════════════════════════════════════════════════════════════════════════════
// FETCHERS
// ═══════════════════════════════════════════════════════════════════════════════

// ── Wikipedia ─────────────────────────────────────────────────────────────────
void fetchWikipedia() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String query = String(itQuery);
  query.replace(" ", "_");
  HTTPClient http;
  String url = "https://en.wikipedia.org/api/rest_v1/page/summary/" + query;
  http.begin(url);
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, http.getString());
    String title   = doc["title"].as<String>();
    String extract = doc["extract"].as<String>();
    itSetResult(("WIKIPEDIA: " + title + "\n\n" + extract).c_str());
  } else if (code == 404) {
    itSetResult("No Wikipedia article found.");
  } else {
    itSetResult(("HTTP error: " + String(code)).c_str());
  }
  http.end();
}

// ── Dictionary ────────────────────────────────────────────────────────────────
void fetchDictionary() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String word = String(itQuery);
  word.trim();
  HTTPClient http;
  http.begin("https://api.dictionaryapi.dev/api/v2/entries/en/" + word);
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, http.getString());
    String out = "DICTIONARY: " + word + "\n\n";
    JsonArray entries = doc.as<JsonArray>();
    int defCount = 0;
    for (JsonObject entry : entries) {
      JsonArray meanings = entry["meanings"];
      for (JsonObject meaning : meanings) {
        String pos = meaning["partOfSpeech"].as<String>();
        out += "[" + pos + "]\n";
        JsonArray defs = meaning["definitions"];
        for (int i = 0; i < 2 && i < (int)defs.size(); i++) {
          out += String(i+1) + ". " + defs[i]["definition"].as<String>() + "\n";
          if (!defs[i]["example"].isNull()) {
            out += "e.g. " + defs[i]["example"].as<String>() + "\n";
          }
          defCount++;
        }
        out += "\n";
        if (defCount >= 4) break;
      }
      if (defCount >= 4) break;
    }
    itSetResult(out.c_str());
  } else {
    itSetResult("Word not found.");
  }
  http.end();
}

// ── Weather (wttr.in) ─────────────────────────────────────────────────────────
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String loc = String(itQuery);
  loc.replace(" ", "+");
  HTTPClient http;
  http.begin("https://wttr.in/" + loc + "?format=j1");
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, http.getString());
    JsonObject cur = doc["current_condition"][0];
    String desc  = cur["weatherDesc"][0]["value"].as<String>();
    String tempC = cur["temp_C"].as<String>();
    String tempF = cur["temp_F"].as<String>();
    String feels = cur["FeelsLikeC"].as<String>();
    String humid = cur["humidity"].as<String>();
    String wind  = cur["windspeedKmph"].as<String>();
    String windD = cur["winddir16Point"].as<String>();
    String vis   = cur["visibility"].as<String>();

    String area    = doc["nearest_area"][0]["areaName"][0]["value"].as<String>();
    String country = doc["nearest_area"][0]["country"][0]["value"].as<String>();

    String out = "WEATHER: " + area + ", " + country + "\n\n";
    out += desc + "\n";
    out += "Temp: " + tempC + "C / " + tempF + "F\n";
    out += "Feels: " + feels + "C\n";
    out += "Humidity: " + humid + "%\n";
    out += "Wind: " + wind + "km/h " + windD + "\n";
    out += "Visibility: " + vis + "km\n\n";

    // 3-day forecast
    out += "FORECAST:\n";
    JsonArray days = doc["weather"];
    for (int d = 0; d < (int)days.size() && d < 3; d++) {
      String date  = days[d]["date"].as<String>();
      String maxC  = days[d]["maxtempC"].as<String>();
      String minC  = days[d]["mintempC"].as<String>();
      String dDesc = days[d]["hourly"][4]["weatherDesc"][0]["value"].as<String>();
      out += date + ": " + dDesc + " " + minC + "-" + maxC + "C\n";
    }
    itSetResult(out.c_str());
  } else {
    itSetResult(("Weather error: HTTP " + String(code)).c_str());
  }
  http.end();
}

// ── Sunrise / Sunset ──────────────────────────────────────────────────────────
void fetchSunrise() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }

  // First geocode the query to lat/lon using Nominatim
  String loc = String(itQuery);
  loc.replace(" ", "+");
  HTTPClient http;
  http.begin("https://nominatim.openstreetmap.org/search?q=" + loc + "&format=json&limit=1");
  http.addHeader("User-Agent", "HitslashRadio/1.0");
  int code = http.GET();
  float lat = 0, lon = 0;
  String placeName = itQuery;
  if (code == 200) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getString());
    if (doc.size() > 0) {
      lat = doc[0]["lat"].as<float>();
      lon = doc[0]["lon"].as<float>();
      placeName = doc[0]["display_name"].as<String>().substring(0, 40);
    } else {
      itSetResult("Location not found.");
      http.end(); return;
    }
  }
  http.end();

  // Now get sunrise/sunset
  String url = "https://api.sunrise-sunset.org/json?lat=" + String(lat, 4)
             + "&lng=" + String(lon, 4) + "&formatted=0";
  http.begin(url);
  code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
    JsonObject res = doc["results"];
    String sunrise = res["sunrise"].as<String>();
    String sunset  = res["sunset"].as<String>();
    String solar   = res["solar_noon"].as<String>();
    String dayLen  = res["day_length"].as<String>();

    // Trim ISO timestamps to just time portion HH:MM:SS UTC
    if (sunrise.length() > 10) sunrise = sunrise.substring(11, 19);
    if (sunset.length() > 10)  sunset  = sunset.substring(11, 19);
    if (solar.length() > 10)   solar   = solar.substring(11, 19);

    String out = "SUNRISE/SUNSET\n";
    out += placeName + "\n\n";
    out += "Sunrise:    " + sunrise + " UTC\n";
    out += "Solar noon: " + solar   + " UTC\n";
    out += "Sunset:     " + sunset  + " UTC\n";
    out += "Day length: " + dayLen  + "s\n";
    itSetResult(out.c_str());
  } else {
    itSetResult("Sunrise API error.");
  }
  http.end();
}

// ── UK Postcode ───────────────────────────────────────────────────────────────
void fetchPostcode() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String pc = String(itQuery);
  pc.replace(" ", "");
  HTTPClient http;
  http.begin("https://api.postcodes.io/postcodes/" + pc);
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, http.getString());
    JsonObject r = doc["result"];
    String out = "POSTCODE: " + r["postcode"].as<String>() + "\n\n";
    out += "Region: "    + r["region"].as<String>() + "\n";
    out += "District: "  + r["admin_district"].as<String>() + "\n";
    out += "Ward: "      + r["admin_ward"].as<String>() + "\n";
    out += "Country: "   + r["country"].as<String>() + "\n";
    out += "Lat: "       + r["latitude"].as<String>() + "\n";
    out += "Lon: "       + r["longitude"].as<String>() + "\n";
    out += "Constituency: " + r["parliamentary_constituency"].as<String>() + "\n";
    out += "NHS CCG: "   + r["ccg"].as<String>() + "\n";
    itSetResult(out.c_str());
  } else if (code == 404) {
    itSetResult("Postcode not found.");
  } else {
    itSetResult(("HTTP " + String(code)).c_str());
  }
  http.end();
}

// ── Country info ──────────────────────────────────────────────────────────────
void fetchCountry() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String cn = String(itQuery);
  cn.replace(" ", "%20");
  HTTPClient http;
  http.begin("https://restcountries.com/v3.1/name/" + cn + "?fullText=false");
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, http.getString());
    JsonObject c = doc[0];
    String name    = c["name"]["common"].as<String>();
    String capital = c["capital"][0].as<String>();
    String region  = c["region"].as<String>();
    String subReg  = c["subregion"].as<String>();
    String pop     = String((long)c["population"].as<long>());
    String area    = String((long)c["area"].as<long>());
    String currency = "";
    for (JsonPair kv : c["currencies"].as<JsonObject>()) {
      currency = kv.value()["name"].as<String>() + " (" + kv.key().c_str() + ")";
      break;
    }
    String lang = "";
    for (JsonPair kv : c["languages"].as<JsonObject>()) {
      if (lang.length()) lang += ", ";
      lang += kv.value().as<String>();
      if (lang.length() > 30) break;
    }
    String tld = c["tld"][0].as<String>();
    String calCode = c["idd"]["root"].as<String>() + c["idd"]["suffixes"][0].as<String>();
    String out = "COUNTRY: " + name + "\n\n";
    out += "Capital:    " + capital + "\n";
    out += "Region:     " + region + " / " + subReg + "\n";
    out += "Population: " + pop + "\n";
    out += "Area:       " + area + " km2\n";
    out += "Currency:   " + currency + "\n";
    out += "Language:   " + lang + "\n";
    out += "TLD:        " + tld + "\n";
    out += "Dial code:  " + calCode + "\n";
    itSetResult(out.c_str());
  } else {
    itSetResult("Country not found.");
  }
  http.end();
}

// ── IP Lookup ─────────────────────────────────────────────────────────────────
void fetchIPLookup() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String ip = String(itQuery);
  ip.trim();
  HTTPClient http;
  // If blank or "me", look up own IP
  String url = (ip.length() == 0 || ip == "ME")
             ? "http://ip-api.com/json"
             : "http://ip-api.com/json/" + ip;
  http.begin(url);
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getString());
    String out = "IP LOOKUP\n\n";
    out += "IP:      " + doc["query"].as<String>()   + "\n";
    out += "Country: " + doc["country"].as<String>() + "\n";
    out += "Region:  " + doc["regionName"].as<String>() + "\n";
    out += "City:    " + doc["city"].as<String>()    + "\n";
    out += "ISP:     " + doc["isp"].as<String>()     + "\n";
    out += "Org:     " + doc["org"].as<String>()     + "\n";
    out += "Lat:     " + doc["lat"].as<String>()     + "\n";
    out += "Lon:     " + doc["lon"].as<String>()     + "\n";
    out += "TZ:      " + doc["timezone"].as<String>()+ "\n";
    itSetResult(out.c_str());
  } else {
    itSetResult("IP lookup failed.");
  }
  http.end();
}

// ── Domain lookup ─────────────────────────────────────────────────────────────
void fetchDomain() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  // Use domainsdb.info for basic domain info + whois-style data
  String domain = String(itQuery);
  domain.trim();
  // Strip protocol if pasted in
  if (domain.startsWith("HTTP://"))  domain = domain.substring(7);
  if (domain.startsWith("HTTPS://")) domain = domain.substring(8);

  // Split into name and TLD
  int dot = domain.indexOf('.');
  String name = domain.substring(0, dot);
  String tld  = domain.substring(dot + 1);

  HTTPClient http;
  http.begin("https://api.domainsdb.info/v1/domains/search?domain=" + name + "&zone=" + tld + "&limit=3");
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, http.getString());
    JsonArray domains = doc["domains"];
    if (domains.size() == 0) {
      itSetResult("No domain info found.");
      http.end(); return;
    }
    String out = "DOMAIN LOOKUP\n\n";
    for (JsonObject d : domains) {
      out += "Domain:  " + d["domain"].as<String>() + "\n";
      out += "Created: " + d["create_date"].as<String>() + "\n";
      out += "Updated: " + d["update_date"].as<String>() + "\n";
      out += "Country: " + d["country"].as<String>() + "\n";
      out += "\n";
    }
    itSetResult(out.c_str());
  } else {
    itSetResult("Domain lookup failed.");
  }
  http.end();
}

// ── ISBN ──────────────────────────────────────────────────────────────────────
void fetchISBN() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String isbn = String(itQuery);
  isbn.replace(" ", "");
  isbn.replace("-", "");
  HTTPClient http;
  http.begin("https://openlibrary.org/api/books?bibkeys=ISBN:" + isbn + "&format=json&jscmd=data");
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, http.getString());
    String key = "ISBN:" + isbn;
    if (!doc.containsKey(key)) {
      itSetResult("ISBN not found.");
      http.end(); return;
    }
    JsonObject book = doc[key];
    String title     = book["title"].as<String>();
    String publisher = book["publishers"][0]["name"].as<String>();
    String pubDate   = book["publish_date"].as<String>();
    String pages     = book["number_of_pages"].as<String>();
    String authors   = "";
    for (JsonObject a : book["authors"].as<JsonArray>()) {
      if (authors.length()) authors += ", ";
      authors += a["name"].as<String>();
    }
    String subjects = "";
    int sc = 0;
    for (JsonObject s : book["subjects"].as<JsonArray>()) {
      if (sc++) subjects += ", ";
      subjects += s["name"].as<String>();
      if (subjects.length() > 60) break;
    }
    String out = "BOOK: ISBN " + isbn + "\n\n";
    out += "Title:     " + title     + "\n";
    out += "Author(s): " + authors   + "\n";
    out += "Publisher: " + publisher + "\n";
    out += "Published: " + pubDate   + "\n";
    out += "Pages:     " + pages     + "\n";
    out += "Subjects:  " + subjects  + "\n";
    itSetResult(out.c_str());
  } else {
    itSetResult("ISBN lookup failed.");
  }
  http.end();
}

// ── Barcode ───────────────────────────────────────────────────────────────────
void fetchBarcode() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String barcode = String(itQuery);
  barcode.trim();
  HTTPClient http;
  http.begin("https://world.openfoodfacts.org/api/v0/product/" + barcode + ".json");
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, http.getString());
    if (doc["status"].as<int>() == 0) {
      itSetResult("Barcode not found in Open Food Facts.");
      http.end(); return;
    }
    JsonObject p = doc["product"];
    String name     = p["product_name"].as<String>();
    String brand    = p["brands"].as<String>();
    String quantity = p["quantity"].as<String>();
    String nutri    = p["nutriscore_grade"].as<String>();
    nutri.toUpperCase();
    String kcal     = p["nutriments"]["energy-kcal_100g"].as<String>();
    String fat      = p["nutriments"]["fat_100g"].as<String>();
    String carbs    = p["nutriments"]["carbohydrates_100g"].as<String>();
    String protein  = p["nutriments"]["proteins_100g"].as<String>();
    String salt     = p["nutriments"]["salt_100g"].as<String>();
    String cats     = p["categories"].as<String>().substring(0, 60);
    String origin   = p["origins"].as<String>();

    String out = "BARCODE: " + barcode + "\n\n";
    out += "Name:    " + name     + "\n";
    out += "Brand:   " + brand    + "\n";
    out += "Qty:     " + quantity + "\n";
    out += "Nutri-score: " + nutri + "\n\n";
    out += "Per 100g:\n";
    out += "  Kcal:    " + kcal    + "\n";
    out += "  Fat:     " + fat     + "g\n";
    out += "  Carbs:   " + carbs   + "g\n";
    out += "  Protein: " + protein + "g\n";
    out += "  Salt:    " + salt    + "g\n\n";
    out += "Category: " + cats    + "\n";
    out += "Origin:   " + origin  + "\n";
    itSetResult(out.c_str());
  } else {
    itSetResult("Barcode lookup failed.");
  }
  http.end();
}

// ── Currency conversion ───────────────────────────────────────────────────────
// Expects query like "USD GBP" or "100 USD GBP"
void fetchCurrency() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String q = String(itQuery);
  q.trim();

  // Parse: optional amount, then FROM, then TO
  float amount = 1.0;
  String from, to;

  // Check if first token is a number
  int firstSpace = q.indexOf(' ');
  if (firstSpace > 0) {
    String first = q.substring(0, firstSpace);
    if (isdigit(first[0])) {
      amount = first.toFloat();
      q = q.substring(firstSpace + 1);
      q.trim();
    }
  }

  int sp = q.indexOf(' ');
  if (sp > 0) {
    from = q.substring(0, sp);
    to   = q.substring(sp + 1);
    to.trim();
  } else {
    itSetResult("Format: [amount] FROM TO\ne.g. 100 USD GBP");
    return;
  }

  HTTPClient http;
  http.begin("https://api.frankfurter.app/latest?from=" + from + "&to=" + to);
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
    float rate = doc["rates"][to.c_str()].as<float>();
    float result = amount * rate;
    String out = "CURRENCY\n\n";
    out += String(amount, 2) + " " + from + " =\n";
    out += String(result, 4) + " " + to   + "\n\n";
    out += "Rate: 1 " + from + " = " + String(rate, 6) + " " + to + "\n";
    out += "Date: " + doc["date"].as<String>() + "\n";
    itSetResult(out.c_str());
  } else {
    itSetResult("Currency lookup failed.\nCheck currency codes.");
  }
  http.end();
}

// ── Crypto prices ─────────────────────────────────────────────────────────────
void fetchCrypto() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String coin = String(itQuery);
  coin.toLowerCase();
  coin.trim();

  HTTPClient http;
  http.begin("https://api.coingecko.com/api/v3/simple/price?ids=" + coin
           + "&vs_currencies=usd,gbp,eur&include_24hr_change=true&include_market_cap=true");
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getString());
    if (!doc.containsKey(coin)) {
      itSetResult("Coin not found.\nTry full name e.g. BITCOIN");
      http.end(); return;
    }
    JsonObject c = doc[coin];
    float usd    = c["usd"].as<float>();
    float gbp    = c["gbp"].as<float>();
    float eur    = c["eur"].as<float>();
    float ch24   = c["usd_24h_change"].as<float>();
    float mcap   = c["usd_market_cap"].as<float>();
    coin.toUpperCase();
    String out = "CRYPTO: " + coin + "\n\n";
    out += "USD: $" + String(usd, 2) + "\n";
    out += "GBP: £" + String(gbp, 2) + "\n";
    out += "EUR: €" + String(eur, 2) + "\n";
    out += "24h: "  + String(ch24, 2) + "%\n";
    out += "Mkt cap: $" + String(mcap / 1e9, 2) + "B\n";
    itSetResult(out.c_str());
  } else {
    itSetResult("Crypto lookup failed.");
  }
  http.end();
}

// ── VIN decoder (NHTSA — free, no key) ───────────────────────────────────────
void fetchVIN() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String vin = String(itQuery);
  vin.trim();

  // Offline decode first
  String out = "VIN: " + vin + "\n\n";

  // WMI (chars 1-3) country/manufacturer
  char wmi[4] = { vin[0], vin[1], vin[2], '\0' };
  out += "WMI: " + String(wmi) + "\n";

  // Model year (char 10)
  char yearChar = (vin.length() >= 10) ? vin[9] : '?';
  const char yearTable[] = "ABCDEFGHJKLMNPRSTVWXY123456789";
  const int  yearBase[]  = {
    1980,1981,1982,1983,1984,1985,1986,1987,1988,1989,
    1990,1991,1992,1993,1994,1995,1996,1997,1998,1999,
    2000,2001,2002,2003,2004,2005,2006,2007,2008,2009
  };
  int modelYear = 0;
  for (int i = 0; i < 30; i++) {
    if (yearTable[i] == yearChar) { modelYear = yearBase[i]; break; }
  }
  // 2010 cycle repeats A=2010 etc.
  if (modelYear == 0) {
    for (int i = 0; i < 30; i++) {
      if (yearTable[i] == yearChar) { modelYear = yearBase[i] + 30; break; }
    }
  }
  if (modelYear > 0) out += "Model year: " + String(modelYear) + "\n";

  // Plant (char 11)
  if (vin.length() >= 11) out += "Plant code: " + String(vin[10]) + "\n";

  out += "\nFetching NHTSA data...\n\n";

  // Online decode via NHTSA
  HTTPClient http;
  http.begin("https://vpic.nhtsa.dot.gov/api/vehicles/decodevinvalues/" + vin + "?format=json");
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, http.getString());
    JsonObject r = doc["Results"][0];
    auto field = [&](const char* label, const char* key) {
      String v = r[key].as<String>();
      if (v.length() > 0 && v != "null" && v != "Not Applicable") {
        out += String(label) + ": " + v + "\n";
      }
    };
    field("Make",         "Make");
    field("Model",        "Model");
    field("Year",         "ModelYear");
    field("Trim",         "Trim");
    field("Body",         "BodyClass");
    field("Drive",        "DriveType");
    field("Engine",       "EngineModel");
    field("Displacement", "DisplacementL");
    field("Cylinders",    "EngineCylinders");
    field("Fuel",         "FuelTypePrimary");
    field("Trans",        "TransmissionStyle");
    field("GVWR",         "GVWR");
    field("Plant city",   "PlantCity");
    field("Plant country","PlantCountry");
    field("Recall count", "RecallCount");
    itSetResult(out.c_str());
  } else {
    out += "NHTSA lookup failed.";
    itSetResult(out.c_str());
  }
  http.end();
}

// ── Flight lookup (AviationStack) ─────────────────────────────────────────────
void fetchFlight() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  String key = AVIATION_API_KEY;
  if (key.length() == 0) {
    itSetResult("Aviation API key not set.\n\nGet a free key at:\naviationstack.com/signup/free\n\nNo credit card needed.\nAdd key to InfoTerminal.h");
    return;
  }
  String flight = String(itQuery);
  flight.trim();
  HTTPClient http;
  http.begin("http://api.aviationstack.com/v1/flights?access_key=" + key + "&flight_iata=" + flight + "&limit=1");
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, http.getString());
    JsonArray data = doc["data"];
    if (data.size() == 0) { itSetResult("Flight not found."); http.end(); return; }
    JsonObject f = data[0];
    String out = "FLIGHT: " + f["flight"]["iata"].as<String>() + "\n\n";
    out += "Airline: " + f["airline"]["name"].as<String>() + "\n";
    out += "Status:  " + f["flight_status"].as<String>() + "\n\n";
    out += "DEP: " + f["departure"]["airport"].as<String>() + "\n";
    out += "  IATA:     " + f["departure"]["iata"].as<String>() + "\n";
    out += "  Sched:    " + f["departure"]["scheduled"].as<String>().substring(11,16) + " UTC\n";
    out += "  Actual:   " + f["departure"]["actual"].as<String>().substring(11,16) + " UTC\n";
    out += "  Terminal: " + f["departure"]["terminal"].as<String>() + "\n";
    out += "  Gate:     " + f["departure"]["gate"].as<String>() + "\n\n";
    out += "ARR: " + f["arrival"]["airport"].as<String>() + "\n";
    out += "  IATA:     " + f["arrival"]["iata"].as<String>() + "\n";
    out += "  Sched:    " + f["arrival"]["scheduled"].as<String>().substring(11,16) + " UTC\n";
    out += "  Estimated:" + f["arrival"]["estimated"].as<String>().substring(11,16) + " UTC\n";
    out += "  Terminal: " + f["arrival"]["terminal"].as<String>() + "\n";
    out += "  Baggage:  " + f["arrival"]["baggage"].as<String>() + "\n";
    itSetResult(out.c_str());
  } else {
    itSetResult(("Flight API error: HTTP " + String(code)).c_str());
  }
  http.end();
}

// ── This day in history ───────────────────────────────────────────────────────
void fetchHistoryToday() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }

  // Step 1: get today's date from time API
  HTTPClient http;
  http.begin("https://timeapi.io/api/v1/time/current/utc");
  int code = http.GET();
  int month = 0, day = 0;
  if (code == 200) {
    StaticJsonDocument<256> tdoc;
    deserializeJson(tdoc, http.getString());
    // utc_time format: "2026-03-11T11:02:38.4193739Z"
    String dt = tdoc["utc_time"].as<String>();
    month = dt.substring(5, 7).toInt();
    day   = dt.substring(8, 10).toInt();
  }
  http.end();

  if (month == 0 || day == 0) {
    itSetResult("Could not get today's date.");
    return;
  }

  // Step 2: fetch Wikipedia On This Day
  String url = "https://en.wikipedia.org/api/rest_v1/feed/onthisday/events/"
             + String(month) + "/" + String(day);
  http.begin(url);
  http.addHeader("User-Agent", "HitslashRadio/1.0");
  code = http.GET();

  if (code == 200) {
    String payload = http.getString();
    http.end();

    StaticJsonDocument<200> filter;
    filter["events"][0]["year"] = true;
    filter["events"][0]["text"] = true;

    DynamicJsonDocument doc(6144);
    DeserializationError err = deserializeJson(doc, payload,
                                               DeserializationOption::Filter(filter));
    if (err) {
      itSetResult(("History parse error:\n" + String(err.c_str())).c_str());
      return;
    }

    JsonArray events = doc["events"];
    if (events.isNull() || events.size() == 0) {
      itSetResult("No events found for today.");
      return;
    }

    String out = "THIS DAY IN HISTORY\n";
    out += String(day) + "/" + String(month) + "\n\n";
    int count = 0;
    for (JsonObject e : events) {
      out += e["year"].as<String>() + ":\n" + e["text"].as<String>() + "\n\n";
      if (++count >= 6) break;
    }
    itSetResult(out.c_str());
  } else {
    http.end();
    itSetResult(("History API failed.\nHTTP " + String(code)).c_str());
  }
}

// ── Random fact ───────────────────────────────────────────────────────────────
void fetchRandomFact() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }
  HTTPClient http;
  http.begin("https://uselessfacts.jsph.pl/api/v2/facts/random?language=en");
  http.addHeader("Accept", "application/json");
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
    String fact = doc["text"].as<String>();
    itSetResult(("RANDOM FACT\n\n" + fact).c_str());
  } else {
    itSetResult("Fact fetch failed.");
  }
  http.end();
}

// Lego set info
void fetchLegoSetInfo() {
  if (WiFi.status() != WL_CONNECTED) { 
    itSetResult("No WiFi."); 
    return; 
  }
  
  String key = rebrickableKey;
  if (key.length() == 0) {
    itSetResult("Rebrickable API key not set.");
    return;
  }
  
  String setNum = String(itQuery);
  if (!setNum.endsWith("-1")) {
    setNum += "-1";
  }
  setNum.trim();
  
  if (setNum.length() == 0) {
    itSetResult("No set number provided.");
    return;
  }
  
  HTTPClient http;
  
  // Construct the correct API URL
  String url = "https://rebrickable.com/api/v3/lego/sets/" + setNum + "/";
  http.begin(url);
  
  // Add the required Authorization header
  http.addHeader("Authorization", "key " + key);
  http.addHeader("Accept", "application/json");
  
  int code = http.GET();
  
  if (code == 200) {
    // Parse the JSON response
    String response = http.getString();
    DynamicJsonDocument doc(2048); // 2KB should be enough for this response
    deserializeJson(doc, response);
    
    // Extract the set information (response is a single object, not an array)
    const char* setName = doc["name"];
    int year = doc["year"];
    int numParts = doc["num_parts"];
    const char* setNum = doc["set_num"];
    const char* imgUrl = doc["set_img_url"];
    
    // Format the output string
    String out = "Set: " + String(setName) + "\n";
    out += "Number: " + String(setNum) + "\n";
    out += "Year: " + String(year) + "\n";
    out += "Parts: " + String(numParts);
    
    itSetResult(out.c_str());
    
  } else if (code == 404) {
    itSetResult("Set not found.");
  } else {
    itSetResult(("API error: HTTP " + String(code)).c_str());
  }
  
  http.end();
}


// ── ESV Bible ─────────────────────────────────────────────────────────────────
void fetchESVBible() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }

  String ref = String(itQuery);
  ref.trim();
  ref.replace(" ", "+");

  HTTPClient http;
  String url = "https://api.esv.org/v3/passage/text/?q=" + ref
             + "&include-headings=false"
             + "&include-footnotes=false"
             + "&include-verse-numbers=true"
             + "&include-short-copyright=false"
             + "&include-passage-references=true";
  http.begin(url);
  http.addHeader("Authorization", "Token " + String(ESV_API_KEY));
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, http.getString());
    String canonical = doc["canonical"].as<String>();
    String passage   = doc["passages"][0].as<String>();
    passage.trim();
    itSetResult(("ESV: " + canonical + "\n\n" + passage).c_str());
  } else if (code == 404) {
    itSetResult("Passage not found.\nCheck reference format.\ne.g. JOHN 3:16");
  } else {
    itSetResult(("ESV API error: HTTP " + String(code)).c_str());
  }
  http.end();
}

// ── OpenAI Ask ────────────────────────────────────────────────────────────────
void fetchOpenAIAsk() {
  if (WiFi.status() != WL_CONNECTED) { itSetResult("No WiFi."); return; }

  String question = String(itQuery);
  question.trim();
  if (question.length() == 0) {
    itSetResult("No question entered.");
    return;
  }

  // Escape any quotes in the question to keep JSON valid
  question.replace("\"", "'");

  String body = "{\"model\":\"gpt-4o-mini\","
                "\"max_tokens\":120,"
                "\"messages\":["
                  "{\"role\":\"system\","
                   "\"content\":\"You are a concise assistant on a tiny OLED screen. "
                   "Reply in plain text, no markdown, 3 sentences maximum.\"},"
                  "{\"role\":\"user\","
                   "\"content\":\"" + question + "\"}"
                "]}";

  HTTPClient http;
  http.begin("https://api.openai.com/v1/chat/completions");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));
  int code = http.POST(body);
  if (code == 200) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getString());
    String reply = doc["choices"][0]["message"]["content"].as<String>();
    reply.trim();
    itSetResult(("ASK AI\n\nQ: " + String(itQuery) + "\n\nA: " + reply).c_str());
  } else {
    DynamicJsonDocument edoc(512);
    String err = http.getString();
    if (!deserializeJson(edoc, err)) {
      String msg = edoc["error"]["message"].as<String>();
      if (msg.length() > 0) {
        itSetResult(("AI error:\n" + msg.substring(0, 200)).c_str());
        http.end(); return;
      }
    }
    itSetResult(("AI error: HTTP " + String(code)).c_str());
  }
  http.end();
}

// ═══════════════════════════════════════════════════════════════════════════════
// PATTERN DETECTION HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

bool itLooksLikeIP(const char* s) {
  // 4 groups of digits separated by dots
  int dots = 0, digits = 0;
  for (int i = 0; s[i]; i++) {
    if (s[i] == '.') dots++;
    else if (isdigit(s[i])) digits++;
    else return false;
  }
  return dots == 3 && digits >= 4;
}

bool itLooksLikeVIN(const char* s) {
  int len = strlen(s);
  if (len != 17) return false;
  for (int i = 0; i < len; i++) {
    if (!isalnum(s[i])) return false;
    char c = toupper(s[i]);
    if (c=='I' || c=='O' || c=='Q') return false; // VINs exclude I, O, Q
  }
  return true;
}

bool itLooksLikeISBN(const char* s) {
  // 10 or 13 digits (ignoring dashes/spaces)
  int count = 0;
  for (int i = 0; s[i]; i++) {
    if (isdigit(s[i]) || (s[i]=='X' && i == (int)strlen(s)-1)) count++;
    else if (s[i] != '-' && s[i] != ' ') return false;
  }
  return count == 10 || count == 13;
}

bool itLooksLikeBarcode(const char* s) {
  int len = strlen(s);
  if (len < 8 || len > 14) return false;
  for (int i = 0; i < len; i++) if (!isdigit(s[i])) return false;
  return true;
}

bool itLooksLikeFlight(const char* s) {
  // 2 letters then 1-4 digits e.g. BA123 EZY1234
  int len = strlen(s);
  if (len < 3 || len > 7) return false;
  if (!isalpha(s[0]) || !isalpha(s[1])) return false;
  for (int i = 2; i < len; i++) if (!isdigit(s[i])) return false;
  return true;
}

bool itLooksLikeCurrency(const char* s) {
  // e.g. "USD GBP" or "100 USD GBP"
  String q = String(s);
  q.trim();
  int spaces = 0;
  for (int i = 0; i < (int)q.length(); i++) if (q[i] == ' ') spaces++;
  return spaces >= 1 && spaces <= 2 && q.length() >= 6;
}

bool itLooksLikeCrypto(const char* s) {
  const char* coins[] = { "BITCOIN", "ETHEREUM", "BTC", "ETH", "DOGE",
                           "LITECOIN", "LTC", "SOLANA", "SOL", "XRP",
                           "CARDANO", "ADA", "POLKADOT", "DOT", nullptr };
  String q = String(s);
  q.toUpperCase();
  q.trim();
  for (int i = 0; coins[i]; i++) if (q == coins[i]) return true;
  return false;
}

bool itLooksLikeUKPostcode(const char* s) {
  // Rough UK postcode check: starts with 1-2 letters, ends with digit+2letters
  String q = String(s);
  q.replace(" ", "");
  int len = q.length();
  if (len < 5 || len > 8) return false;
  if (!isalpha(q[0])) return false;
  // ends with digit+letter+letter
  if (!isalpha(q[len-1]) || !isalpha(q[len-2]) || !isdigit(q[len-3])) return false;
  return true;
}

bool itLooksLikeCountry(const char* s) {
  const char* countries[] = {
    "FRANCE","GERMANY","SPAIN","ITALY","JAPAN","CHINA","INDIA","BRAZIL",
    "USA","CANADA","AUSTRALIA","RUSSIA","UK","MEXICO","ARGENTINA","EGYPT",
    "NIGERIA","KENYA","SOUTH AFRICA","NEW ZEALAND","SWEDEN","NORWAY",
    "DENMARK","FINLAND","NETHERLANDS","BELGIUM","SWITZERLAND","AUSTRIA",
    "POLAND","PORTUGAL","GREECE","TURKEY","IRAN","IRAQ","SAUDI ARABIA",
    "ISRAEL","PAKISTAN","BANGLADESH","SRI LANKA","THAILAND","VIETNAM",
    "INDONESIA","PHILIPPINES","SOUTH KOREA","NORTH KOREA","UKRAINE", nullptr
  };
  String q = String(s);
  q.toUpperCase();
  q.trim();
  for (int i = 0; countries[i]; i++) if (q == countries[i]) return true;
  return false;
}

bool itLooksLikeDomain(const char* s) {
  String q = String(s);
  q.trim();
  // Contains a dot but isn't an IP
  int dot = q.indexOf('.');
  if (dot < 1) return false;
  if (itLooksLikeIP(s)) return false;
  // Doesn't contain spaces
  if (q.indexOf(' ') >= 0) return false;
  return true;
}

// ── Names for display ─────────────────────────────────────────────────────────
const char* queryTypeName(InfoQueryType qt) {
  switch (qt) {
    case IQ_WIKIPEDIA:    return "Wikipedia";
    case IQ_DICTIONARY:   return "Dictionary";
    case IQ_WEATHER:      return "Weather";
    case IQ_SUNRISE:      return "Sunrise / Sunset";
    case IQ_POSTCODE:     return "UK Postcode";
    case IQ_COUNTRY:      return "Country Info";
    case IQ_IP_LOOKUP:    return "IP Lookup";
    case IQ_DOMAIN:       return "Domain Lookup";
    case IQ_ISBN:         return "ISBN / Book";
    case IQ_BARCODE:      return "Barcode / Product";
    case IQ_CURRENCY:     return "Currency Convert";
    case IQ_CRYPTO:       return "Crypto Price";
    case IQ_VIN:          return "VIN Decoder";
    case IQ_FLIGHT:       return "Flight Lookup";
    case IQ_HISTORY_TODAY:return "This Day in History";
    case IQ_RANDOM_FACT:  return "Random Fact";
    case IQ_LEGO_SET_INFO: return "Lego Set Info";
    case IQ_ESV_BIBLE:     return "ESV Bible";
    case IQ_OPENAI_ASK:    return "Ask AI";
    default:              return "Unknown";
  }
}

// ── Result buffer helpers ─────────────────────────────────────────────────────
void itSetResult(const char* text) {
  strncpy(itResult, text, IT_MAX_RESULT_LEN - 1);
  itResult[IT_MAX_RESULT_LEN - 1] = '\0';
}

void itAppendResult(const char* text) {
  int remaining = IT_MAX_RESULT_LEN - 1 - strlen(itResult);
  strncat(itResult, text, remaining);
}