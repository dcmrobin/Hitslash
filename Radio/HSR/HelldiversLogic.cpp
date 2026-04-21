#include "HelldiversLogic.h"

String majorOrderTitle = "";
String majorOrderBrief = "";
String rewardText = "";
String objectiveText[MAX_OBJECTIVES];
int objectiveProgress[MAX_OBJECTIVES];
int objectiveTarget[MAX_OBJECTIVES];
int objectiveCount = 0;
int contentHeight = 0;

int inputSequence[SEQ_LENGTH];
int seqIndex = 0;
unsigned long lastSeqPressTime = 0;
const unsigned long seqTimeout = 2000; // ms — reset sequence if no press within this time

int secretSequence[SEQ_LENGTH] = {
  BTN_UP,
  BTN_RIGHT,
  BTN_DOWN,
  BTN_DOWN,
  BTN_DOWN
};

String hdTitle = "";
String hdBrief = "";
String hdTask = "";
String newsItems[MAX_NEWS_ITEMS] = {};
int hdProgress = 0;
long hdExpires = 0;
unsigned long lastMajorOrderUpdate = 0;
unsigned long lastNewsUpdate = 0;
const unsigned long hdUpdateInterval = 60000;
unsigned long refreshPressStart = 0;
unsigned long lastRefreshTrigger = 0;
const unsigned long longPressTime = 2000;
const unsigned long refreshInterval = 1000;
int hdScrollOffset = 0;

// ================= API FETCH =========================

void fetchMajorOrder(bool force) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!force && millis() - lastMajorOrderUpdate < hdUpdateInterval) return;

  lastMajorOrderUpdate = millis();

  HTTPClient http;
  http.begin("https://api.helldivers2.dev/raw/api/v2/Assignment/War/801");
  http.addHeader("X-Super-Client", "HitslashRadio");
  http.addHeader("X-Super-Contact", "dcm.robin@gmail.com");
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      parseMajorOrder(doc);
      JsonObject order = doc[0];
      hdTitle    = order["setting"]["overrideTitle"].as<String>();
      hdBrief    = order["setting"]["overrideBrief"].as<String>();
      hdTask     = order["setting"]["taskDescription"].as<String>();
      hdProgress = order["progress"][0];
      hdExpires  = order["expiresIn"];
    } else {
      majorOrderTitle = "API Parse Error";
      majorOrderBrief = String(err.c_str());
      rewardText = "";
      objectiveCount = 0;
    }
  } else {
    majorOrderTitle = "API Error";
    majorOrderBrief = "HTTP " + String(httpCode);
    rewardText = "";
    objectiveCount = 0;
  }
  http.end();
}

void fetchNews(bool force) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!force && millis() - lastNewsUpdate < hdUpdateInterval) return;

  lastNewsUpdate = millis();

  HTTPClient http;
  http.begin("https://api.helldivers2.dev/raw/api/NewsFeed/801");
  http.addHeader("X-Super-Client", "HitslashRadio");
  http.addHeader("X-Super-Contact", "dcm.robin@gmail.com");
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      parseNews(doc);
    }
  }
  http.end();
}

// ================= SECRET SEQUENCE ===================

void registerSequence(int btn) {
  unsigned long now = millis();

  // If too long since last press, start fresh
  if (now - lastSeqPressTime > seqTimeout) {
    seqIndex = 0;
  }
  lastSeqPressTime = now;

  inputSequence[seqIndex++] = btn;

  if (seqIndex >= SEQ_LENGTH) {
    bool match = true;
    for (int i = 0; i < SEQ_LENGTH; i++) {
      if (inputSequence[i] != secretSequence[i]) {
        match = false;
        break;
      }
    }

    if (match && !offlineMode) {
      display.clearDisplay();
      display.setCursor(17, 30);
      display.print("HELLDIVERS 2");
      display.setCursor(17, 40);
      display.print("WAR TERMINAL");
      display.setCursor(10, 60);
      display.print("Receiving data...");
      display.display();

      currentMode    = MODE_HELLDIVERS;
      currentDisplay = DISPLAY_MAJOR_ORDER;
      hdScrollOffset = 0;

      fetchMajorOrder(true);
      fetchNews(true);
      drawHelldiversMajorOrder();
    }

    seqIndex = 0;
  }
}

// Reads from the shared buttons[] array — no direct digitalRead calls.
void checkSecretSequence() {
  // Map BTN_IDX_* to the BTN_* pin values used in secretSequence
  struct { int idx; int pin; } btnMap[] = {
    {BTN_IDX_UP,    BTN_UP},
    {BTN_IDX_RIGHT, BTN_RIGHT},
    {BTN_IDX_DOWN,  BTN_DOWN},
    {BTN_IDX_LEFT,  BTN_LEFT},
  };

  for (auto &b : btnMap) {
    if (buttons[b.idx].pressed) {
      registerSequence(b.pin);
    }
  }
}

// ================= HELLDIVERS BUTTON HANDLING ========

void handleHelldiversButtons() {
  static unsigned long lastScrollTime = 0;
  const unsigned long scrollRepeatDelay = 120;

  // REFRESH: hold 2 s to exit, hold shorter to force refresh
  if (buttons[BTN_IDX_REFRESH].held) {
    if (buttons[BTN_IDX_REFRESH].pressed) {
      refreshHeld = true;
      refreshPressStart  = buttons[BTN_IDX_REFRESH].pressTime;
      lastRefreshTrigger = millis();
    }

    // Long press — exit to radio
    if (millis() - refreshPressStart > longPressTime) {
      currentMode    = MODE_RADIO;
      currentDisplay = DISPLAY_STATION;
      drawRadioScreen();
      refreshHeld = false;
      return;
    }

    // Short repeated refreshes while held
    if (millis() - lastRefreshTrigger > refreshInterval) {
      if (currentDisplay == DISPLAY_MAJOR_ORDER) {
        fetchMajorOrder(true);
        drawHelldiversMajorOrder();
      } else if (currentDisplay == DISPLAY_NEWS) {
        fetchNews(true);
        drawHelldiversNews();
      }
      lastRefreshTrigger = millis();
    }
  } else {
    if (buttons[BTN_IDX_REFRESH].released) refreshHeld = false;
  }

  // DOWN — scroll down (held = repeat)
  if (buttons[BTN_IDX_DOWN].held) {
    if (buttons[BTN_IDX_DOWN].pressed || millis() - lastScrollTime > scrollRepeatDelay) {
      hdScrollOffset -= 10;
      int minScroll = -(contentHeight);
      if (minScroll > 0) minScroll = 0;
      hdScrollOffset = constrain(hdScrollOffset, minScroll, 0);
      if (currentDisplay == DISPLAY_MAJOR_ORDER) drawHelldiversMajorOrder();
      else if (currentDisplay == DISPLAY_NEWS)   drawHelldiversNews();
      lastScrollTime = millis();
    }
  }

  // UP — scroll up (held = repeat)
  if (buttons[BTN_IDX_UP].held) {
    if (buttons[BTN_IDX_UP].pressed || millis() - lastScrollTime > scrollRepeatDelay) {
      hdScrollOffset += 10;
      int minScroll = -(contentHeight);
      if (minScroll > 0) minScroll = 0;
      hdScrollOffset = constrain(hdScrollOffset, minScroll, 0);
      if (currentDisplay == DISPLAY_MAJOR_ORDER) drawHelldiversMajorOrder();
      else if (currentDisplay == DISPLAY_NEWS)   drawHelldiversNews();
      lastScrollTime = millis();
    }
  }

  // LEFT / RIGHT — switch between Major Order and News
  if (buttons[BTN_IDX_LEFT].pressed || buttons[BTN_IDX_RIGHT].pressed) {
    currentDisplay = (currentDisplay == DISPLAY_MAJOR_ORDER)
                     ? DISPLAY_NEWS
                     : DISPLAY_MAJOR_ORDER;
    hdScrollOffset = 0;
  }
}

// ================= JSON PARSING ======================

void parseMajorOrder(JsonDocument &doc) {
  objectiveCount = 0;

  int foundIndex = -1;
  for (size_t i = 0; i < doc.size(); i++) {
    if (doc[i].containsKey("setting") && doc[i]["setting"].containsKey("overrideTitle")) {
      String title = doc[i]["setting"]["overrideTitle"].as<String>();
      if (title == "MAJOR ORDER") {
        foundIndex = i;
        break;
      }
    }
  }

  if (foundIndex == -1) {
    majorOrderTitle = "No Major Order";
    majorOrderBrief = "Could not find Major Order assignment.";
    rewardText = "";
    return;
  }

  JsonObject order = doc[foundIndex];
  majorOrderTitle = order["setting"]["overrideTitle"].as<String>();
  majorOrderBrief = order["setting"]["overrideBrief"].as<String>();
  int rewardAmount = order["setting"]["reward"]["amount"] | 0;
  rewardText = "Reward: " + String(rewardAmount) + " Medals";

  if (!order["setting"].containsKey("tasks") || !order.containsKey("progress")) {
    objectiveCount = 0;
    return;
  }

  JsonArray tasks    = order["setting"]["tasks"];
  JsonArray progress = order["progress"];
  int index = 0;

  for (JsonObject task : tasks) {
    if (index >= MAX_OBJECTIVES) break;
    int current = progress[index] | 0;
    int target  = 0;

    JsonArray values = task["values"];
    for (JsonVariant v : values) {
      int val = v.as<int>();
      if (val >= current && val > target) target = val;
    }

    if (current == 0) target = 0;
    else if (current == 1) target = 1;

    objectiveTarget[index]   = target;
    objectiveProgress[index] = current;
    objectiveText[index]     = "Objective " + String(index + 1);
    index++;
  }

  objectiveCount = index;
}

void parseNews(JsonDocument &doc) {
  for (int i = 0; i < MAX_NEWS_ITEMS; i++) newsItems[i] = "";

  int total = doc.size();
  if (total == 0) {
    newsItems[0] = "No news available";
    return;
  }

  int slot = 0;
  for (int i = total - 1; i >= 0 && slot < MAX_NEWS_ITEMS; i--) {
    String msg = doc[i]["message"].as<String>();
    if (msg.length() > 0) newsItems[slot++] = msg;
  }

  while (slot < MAX_NEWS_ITEMS) newsItems[slot++] = "No further news.";
}