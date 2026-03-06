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
int hdProgress = 0;
long hdExpires = 0;
unsigned long lastHDUpdate = 0;
const unsigned long hdUpdateInterval = 60000;
unsigned long refreshPressStart = 0;
unsigned long lastRefreshTrigger = 0;
const unsigned long longPressTime = 2000;     // 2s to exit Helldivers
const unsigned long refreshInterval = 1000;   // 1s between refreshes when held
int hdScrollOffset = 0;

void fetchMajorOrder(bool force) {

  if (WiFi.status() != WL_CONNECTED) return;

  if (!force && millis() - lastHDUpdate < hdUpdateInterval)
    return;

  lastHDUpdate = millis();

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

      currentMode = MODE_HELLDIVERS;
      hdScrollOffset = 0;

      fetchMajorOrder(true);
      drawHelldiversMajorOrder();
    }

    seqIndex = 0;
  }
}

void handleHelldiversButtons() {

  bool refreshPressed = !digitalRead(BTN_REFRESH);

  static bool downWasPressed = false;
  static bool upWasPressed = false;
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
      drawRadioScreen();
      refreshHeld = false;
      return;
    }
    // Controlled refresh while holding
    if (millis() - lastRefreshTrigger > refreshInterval) {
      fetchMajorOrder(true);
      drawHelldiversMajorOrder();
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
      int minScroll = -(contentHeight + 200);
      if (minScroll > 0) minScroll = 0;
      if (hdScrollOffset > 0) hdScrollOffset = 0;
      if (hdScrollOffset < minScroll) hdScrollOffset = minScroll;
      drawHelldiversMajorOrder();
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
      int minScroll = -(contentHeight + 200);
      if (minScroll > 0) minScroll = 0;
      if (hdScrollOffset > 0) hdScrollOffset = 0;
      if (hdScrollOffset < minScroll) hdScrollOffset = minScroll;
      drawHelldiversMajorOrder();
      lastScrollTime = millis();
    }
    upWasPressed = true;
  } else {
    upWasPressed = false;
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
  // Defensive: check structure
  if (!doc[0].containsKey("setting")) {
    majorOrderTitle = "No Data";
    majorOrderBrief = "Major Order data missing.";
    rewardText = "";
    return;
  }
  majorOrderTitle = doc[0]["setting"]["overrideTitle"].as<String>();
  majorOrderBrief = doc[0]["setting"]["overrideBrief"].as<String>();
  int rewardAmount = doc[0]["setting"]["reward"]["amount"] | 0;
  rewardText = "Reward: " + String(rewardAmount) + " Medals";
  if (!doc[0]["setting"].containsKey("tasks") || !doc[0].containsKey("progress")) {
    objectiveCount = 0;
    return;
  }
  JsonArray tasks = doc[0]["setting"]["tasks"];
  JsonArray progress = doc[0]["progress"];
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