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
int secretSequence[SEQ_LENGTH] = {
  BTN_UP,
  BTN_RIGHT,
  BTN_DOWN,
  BTN_DOWN,
  BTN_DOWN
};
bool prevUpState = HIGH;
bool prevRightState = HIGH;
bool prevDownState = HIGH;
bool prevLeftState = HIGH;
unsigned long lastUpDebounce = 0;
unsigned long lastRightDebounce = 0;
unsigned long lastDownDebounce = 0;
unsigned long lastLeftDebounce = 0;
const unsigned long secretDebounceTime = 120; // ms
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
const unsigned long longPressTime = 2000;     // 2s to exit Helldivers
const unsigned long refreshInterval = 1000;   // 1s between refreshes when held
int hdScrollOffset = 0;

void fetchMajorOrder(bool force) {

  if (WiFi.status() != WL_CONNECTED) return;

  if (!force && millis() - lastMajorOrderUpdate < hdUpdateInterval)
    return;

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
      hdTitle = order["setting"]["overrideTitle"].as<String>();
      hdBrief = order["setting"]["overrideBrief"].as<String>();
      hdTask = order["setting"]["taskDescription"].as<String>();
      hdProgress = order["progress"][0];
      hdExpires = order["expiresIn"];
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

  if (!force && millis() - lastNewsUpdate < hdUpdateInterval)
    return;

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

void registerSequence(int btn) {

  inputSequence[seqIndex++] = btn;

  if (seqIndex >= SEQ_LENGTH) {

    bool match = true;

    for (int i = 0; i < SEQ_LENGTH; i++) {
      if (inputSequence[i] != secretSequence[i]) {
        match = false;
        break;
      }
    }

    if (match) {
      display.clearDisplay();
      display.setCursor(17, 30);
      display.print("HELLDIVERS 2");
      display.setCursor(17, 40);
      display.print("WAR TERMINAL");
      display.setCursor(10, 60);
      display.print("Recieving data...");
      display.display();
      currentMode = MODE_HELLDIVERS;
      currentDisplay = DISPLAY_MAJOR_ORDER;
      hdScrollOffset = 0;

      fetchMajorOrder(true);
      fetchNews(true);
      drawHelldiversMajorOrder();
    }

    seqIndex = 0;
  }
}

void handleHelldiversButtons() {

  bool refreshPressed = !digitalRead(BTN_REFRESH);

  static bool downWasPressed = false;
  static bool upWasPressed = false;
  static bool leftWasPressed = false;
  static bool rightWasPressed = false;
  static unsigned long lastScrollTime = 0;
  const unsigned long scrollRepeatDelay = 120; // ms

  // Handle refresh/exit
  if (refreshPressed) {
    if (!refreshHeld) {
      refreshHeld = true;
      refreshPressStart = millis();
      lastRefreshTrigger = millis();
    }
    // Exit if held long enough
    if (millis() - refreshPressStart > longPressTime) {
      currentMode = MODE_RADIO;
      currentDisplay = DISPLAY_STATION;
      drawRadioScreen();
      refreshHeld = false;
      return;
    }
    // Controlled refresh while holding
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
    refreshHeld = false;
  }

  // DOWN scroll (hold to repeat)
  if (!digitalRead(BTN_DOWN)) {
    if (!downWasPressed || millis() - lastScrollTime > scrollRepeatDelay) {
      hdScrollOffset -= 10;
      // Calculate minScroll based on contentHeight
      int minScroll = -(contentHeight + 0);
      if (minScroll > 0) minScroll = 0;
      if (hdScrollOffset > 0) hdScrollOffset = 0;
      if (hdScrollOffset < minScroll) hdScrollOffset = minScroll;
      if (currentDisplay == DISPLAY_MAJOR_ORDER) {
        drawHelldiversMajorOrder();
      } else if (currentDisplay == DISPLAY_NEWS) {
        drawHelldiversNews();
      }
      lastScrollTime = millis();
    }
    downWasPressed = true;
  } else {
    downWasPressed = false;
  }

  // UP scroll (hold to repeat)
  if (!digitalRead(BTN_UP)) {
    if (!upWasPressed || millis() - lastScrollTime > scrollRepeatDelay) {
      hdScrollOffset += 10;
      int minScroll = -(contentHeight + 0);
      if (minScroll > 0) minScroll = 0;
      if (hdScrollOffset > 0) hdScrollOffset = 0;
      if (hdScrollOffset < minScroll) hdScrollOffset = minScroll;
      if (currentDisplay == DISPLAY_MAJOR_ORDER) {
        drawHelldiversMajorOrder();
      } else if (currentDisplay == DISPLAY_NEWS) {
        drawHelldiversNews();
      }
      lastScrollTime = millis();
    }
    upWasPressed = true;
  } else {
    upWasPressed = false;
  }

  if (!digitalRead(BTN_LEFT)) {
    if (!leftWasPressed || millis() - lastScrollTime > scrollRepeatDelay) {
      currentDisplay == DISPLAY_MAJOR_ORDER ? currentDisplay = DISPLAY_NEWS : currentDisplay = DISPLAY_MAJOR_ORDER;
      hdScrollOffset = 0;
      lastScrollTime = millis();
    }
    leftWasPressed = true;
  } else {
    leftWasPressed = false;
  }
  
  if (!digitalRead(BTN_RIGHT)) {
    if (!rightWasPressed || millis() - lastScrollTime > scrollRepeatDelay) {
      currentDisplay == DISPLAY_MAJOR_ORDER ? currentDisplay = DISPLAY_NEWS : currentDisplay = DISPLAY_MAJOR_ORDER;
      hdScrollOffset = 0;
      lastScrollTime = millis();
    }
    rightWasPressed = true;
  } else {
    rightWasPressed = false;
  }
}

void checkSecretSequence() {

  unsigned long now = millis();
  bool upState = digitalRead(BTN_UP);
  bool rightState = digitalRead(BTN_RIGHT);
  bool downState = digitalRead(BTN_DOWN);
  bool leftState = digitalRead(BTN_LEFT);

  // UP pressed
  if (prevUpState == HIGH && upState == LOW && (now - lastUpDebounce > secretDebounceTime)) {
    registerSequence(BTN_UP);
    lastUpDebounce = now;
  }

  // RIGHT pressed
  if (prevRightState == HIGH && rightState == LOW && (now - lastRightDebounce > secretDebounceTime)) {
    registerSequence(BTN_RIGHT);
    lastRightDebounce = now;
  }

  // DOWN pressed
  if (prevDownState == HIGH && downState == LOW && (now - lastDownDebounce > secretDebounceTime)) {
    registerSequence(BTN_DOWN);
    lastDownDebounce = now;
  }

  // LEFT pressed
  if (prevLeftState == HIGH && leftState == LOW && (now - lastLeftDebounce > secretDebounceTime)) {
    registerSequence(BTN_LEFT);
    lastLeftDebounce = now;
  }

  prevUpState = upState;
  prevRightState = rightState;
  prevDownState = downState;
  prevLeftState = leftState;
}

void parseMajorOrder(JsonDocument &doc) {
  objectiveCount = 0;
  // Find the assignment with overrideTitle == "MAJOR ORDER"
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
  JsonArray tasks = order["setting"]["tasks"];
  JsonArray progress = order["progress"];
  int index = 0;
  for (JsonObject task : tasks) {
    if (index >= MAX_OBJECTIVES) break;
    int current = progress[index] | 0;
    int target = 0;
    // Loop through values to find the largest value greater than progress (likely the target)
    JsonArray values = task["values"];
    for (JsonVariant v : values) {
      int val = v.as<int>();
      if (val >= current && val > target) {
        target = val;
      }
    }
    if (current == 0) {
      target = 0;
    } else if (current == 1) {
      target = 1;
    }
    objectiveTarget[index] = target;
    objectiveProgress[index] = current;
    objectiveText[index] = "Objective " + String(index + 1);
    index++;
  }
  objectiveCount = index;
}

void parseNews(JsonDocument &doc) {
  // Clear old news first
  for (int i = 0; i < MAX_NEWS_ITEMS; i++) {
    newsItems[i] = "";
  }

  int total = doc.size();
  if (total == 0) {
    newsItems[0] = "No news available";
    return;
  }

  // API returns items oldest-first, so read backwards from the end for most recent first
  int slot = 0;
  for (int i = total - 1; i >= 0 && slot < MAX_NEWS_ITEMS; i--) {
    String msg = doc[i]["message"].as<String>();
    if (msg.length() > 0) {
      newsItems[slot++] = msg;
    }
  }

  // Fill any remaining slots
  while (slot < MAX_NEWS_ITEMS) {
    newsItems[slot++] = "No further news.";
  }
}