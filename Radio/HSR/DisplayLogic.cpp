#include "DisplayLogic.h"

Adafruit_SH1107 display = Adafruit_SH1107(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);
DisplayMode currentDisplay = DISPLAY_STATION;
String lines[MAX_LINES];
int totalLines = 0;
int scrollOffset = 0;
int lineHeight = 10;
int maxVisibleLines = 12;

void drawProgressBar(int x, int y, int width, int height, float percent) {
  if (percent < 0) percent = 0;
  if (percent > 1) percent = 1;
  display.drawRect(x, y, width, height, SH110X_WHITE);
  int fill = (width - 2) * percent;
  display.fillRect(x + 1, y + 1, fill, height - 2, SH110X_WHITE);
}

void drawWrappedText(String text, int x, int y, int maxWidth, int lineHeight) {

  int cursorX = x;
  int cursorY = y;

  String word = "";

  for (int i = 0; i < text.length(); i++) {

    char c = text[i];

    if (c == ' ' || i == text.length() - 1) {

      if (i == text.length() - 1)
        word += c;

      int wordWidth = word.length() * 6;

      if (cursorX + wordWidth > maxWidth) {
        cursorX = x;
        cursorY += lineHeight;
      }

      display.setCursor(cursorX, cursorY);
      display.print(word + " ");

      cursorX += (word.length() + 1) * 6;

      word = "";
    } 
    else {
      word += c;
    }
  }
}

void drawSetupScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  
  for (int i = 0; i < maxVisibleLines; i++) {
    int lineIndex = i + scrollOffset;
    if (lineIndex >= totalLines) break;
    display.setCursor(0, i * lineHeight);
    display.println(lines[lineIndex]);
  }
  
  // Draw battery in bottom left
  drawBatteryIcon(0, display.height() - 15);
  
  display.display();
}

void drawRadioScreen() {
  display.clearDisplay();
  
  // Station name (large)
  display.setTextSize(2);
  display.setCursor(0, 10);
  if (audio.isRunning()) {
    display.println(stationNames[currentStation]);
  } else if (!audio.isRunning() && millis() - lastReconnect > 5000){
    display.println("ERROR");
  }
  
  // Volume
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.print("Vol: ");
  int vol = audio.getVolume();
  display.print(vol);
  
  // Volume bar
  int barWidth = map(vol, 0, 21, 0, 100);
  display.drawRect(0, 60, 100, 8, SH110X_WHITE);
  display.fillRect(0, 60, barWidth, 8, SH110X_WHITE);
  
  // Hint for button functions
  display.setCursor(0, 80);
  display.print("L/R: Station");
  display.setCursor(0, 92);
  display.print("U/D: Cycle screens");
  
  // Draw battery in bottom left
  drawBatteryIcon(0, display.height() - 15);
  
  display.display();
}

void drawWifiInfoScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  
  display.setCursor(0, 0);
  display.println("WiFi Info");
  display.println("---------");
  
  display.print("SSID: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.println(WiFi.SSID());
  } else {
    display.println("Not Connected");
  }
  
  display.print("RSSI: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.print(WiFi.RSSI());
    display.println(" dBm");
  } else {
    display.println("N/A");
  }
  
  display.print("IP: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.println(WiFi.localIP());
  } else {
    display.println("None");
  }
  
  display.println("---------");
  display.print("Station: ");
  display.println(stationNames[currentStation]);
  
  display.print("Volume: ");
  display.println(audio.getVolume());
  display.print("");
  display.setCursor(0, 80);
  display.print("Hold refresh for");
  display.setCursor(0, 92);
  display.print("network management.");
  
  // Draw battery in bottom left
  drawBatteryIcon(0, display.height() - 15);
  
  display.display();
}

void drawNetworkListScreen() {
  buildNetworkListText();
  
  display.clearDisplay();
  display.setTextSize(1);
  
  for (int i = 0; i < maxVisibleLines; i++) {
    int lineIndex = i + scrollOffset;
    if (lineIndex >= totalLines) break;
    display.setCursor(0, i * lineHeight);
    display.println(lines[lineIndex]);
  }
  
  // Draw battery in bottom left
  drawBatteryIcon(0, display.height() - 15);
  
  display.display();
}

void drawBatteryIcon(int x, int y) {
  // Draw battery outline
  display.drawRect(x, y, 25, 10, SH110X_WHITE);
  display.drawRect(x + 25, y + 2, 3, 6, SH110X_WHITE);
  
  // Fill based on percentage
  float pct = constrain(batteryPercent, 0, 100);
  int fillWidth = map(pct, 0, 100, 0, 23);
  if (fillWidth > 0) {
    if (batteryPercent > 20) {
      display.fillRect(x + 1, y + 1, fillWidth, 8, SH110X_WHITE);
    } else {
      // Low battery - fill with blinking or just outline
      display.fillRect(x + 1, y + 1, fillWidth, 8, SH110X_WHITE);
    }
  }
  
  // Show percentage text
  display.setCursor(x + 30, y);
  display.setTextSize(1);
  display.print((int)batteryPercent);
  display.print("%");
}

void drawHelldiversMajorOrder() {
  display.clearDisplay();
  int y = hdScrollOffset;
  display.setCursor(0, y);
  display.println("HD2 WAR TERMINAL");
  y += 12;
  y = drawWrappedText(majorOrderTitle, 0, y, 128);
  y += 4;
  y = drawWrappedText(majorOrderBrief, 0, y, 128);
  y += 8;
  for (int i = 0; i < objectiveCount; i++) {
    y = drawWrappedText(objectiveText[i], 0, y, 128);
    float percent = 0;
    if (objectiveTarget[i] > 0)
      percent = (float)objectiveProgress[i] / objectiveTarget[i];
    drawProgressBar(0, y, 110, 8, percent);
    y += 10;
    display.setCursor(0, y);
    display.print(objectiveProgress[i]);
    display.print("/");
    display.println(objectiveTarget[i]);
    y += 12;
  }
  y = drawWrappedText(rewardText, 0, y, 128);
  contentHeight = y;
  // Constrain hdScrollOffset so content stays in view
  int minScroll = -(contentHeight + 0);
  if (minScroll > 0) minScroll = 0;
  hdScrollOffset = constrain(hdScrollOffset, minScroll, 0);
  display.display();
}

void drawHelldiversNews() {
  display.clearDisplay();
  String totalNews = "";
  int y = hdScrollOffset;
  display.setCursor(0, y);
  display.println("HD2 WAR TERMINAL");
  y += 12;
  for (int i = 0; i < MAX_NEWS_ITEMS; i++) {
    totalNews += newsItems[i] + "\n\n";
  }
  totalNews += "\n\n\nEnd of most recent news.";
  y = drawWrappedText(totalNews, 0, y, 128);
  contentHeight = y*10;
  // Constrain hdScrollOffset so content stays in view
  int minScroll = -(contentHeight + 0);
  if (minScroll > 0) minScroll = 0;
  hdScrollOffset = constrain(hdScrollOffset, minScroll, 0);
  display.display();
}

int drawWrappedText(String text, int x, int y, int maxWidth) {
  int cursorX = x;
  int cursorY = y;
  String word = "";
  bool inTag = false;

  for (int i = 0; i < text.length(); i++) {
    char c = text[i];

    // Skip anything inside < >
    if (c == '<') {
      inTag = true;
      continue;
    }
    if (c == '>') {
      inTag = false;
      continue;
    }
    if (inTag) continue;

    // Handle newline
    if (c == '\n') {
      if (word.length() > 0) {
        int wordWidth = word.length() * 6;
        if (cursorX + wordWidth > maxWidth) {
          cursorX = x;
          cursorY += 10;
        }
        display.setCursor(cursorX, cursorY);
        display.print(word);
        word = "";
      }

      cursorX = x;
      cursorY += 10;
      continue;
    }

    // Word wrapping
    if (c == ' ' || i == text.length() - 1) {
      if (i == text.length() - 1 && c != ' ') word += c;

      int wordWidth = word.length() * 6;

      if (cursorX + wordWidth > maxWidth) {
        cursorX = x;
        cursorY += 10;
      }

      display.setCursor(cursorX, cursorY);
      display.print(word);

      cursorX += wordWidth + 6;
      word = "";
    }
    else {
      word += c;
    }
  }

  return cursorY + 10;
}

void buildSetupText() {
  totalLines = 0;
  lines[totalLines++] = "HITSLASH RADIO";
  lines[totalLines++] = "Setup Mode";
  lines[totalLines++] = "";
  lines[totalLines++] = "Connect to:";
  lines[totalLines++] = AP_SSID;
  lines[totalLines++] = "";
  lines[totalLines++] = "Captive portal";
  lines[totalLines++] = "should appear.";
  lines[totalLines++] = "";
  lines[totalLines++] = "If not open:";
  lines[totalLines++] = "192.168.4.1";
}

void buildNetworkListText() {
  totalLines = 0;
  lines[totalLines++] = "Saved Networks";
  lines[totalLines++] = "-------------";
  
  if (savedCount == 0) {
    lines[totalLines++] = "No networks saved";
  } else {
    for (int i = 0; i < savedCount; i++) {
      String line = String(i+1) + ". " + savedSSIDs[i];
      if (i == selectedNetworkIndex) {
        line = "> " + line;
      }
      lines[totalLines++] = line;
    }
  }
  
  lines[totalLines++] = "";
  lines[totalLines++] = "UP/DOWN: Select";
  lines[totalLines++] = "LEFT: Delete";
  lines[totalLines++] = "RIGHT: Clear All";
  lines[totalLines++] = "Hold refresh restart";
}

void buildConnectingText(const char* message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Connecting...");
  display.setCursor(0, 35);
  display.println(message);
  display.display();
}