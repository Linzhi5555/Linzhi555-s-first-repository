/*
 * Project: Jie - ESP32-C3 Virtual Pet
 *
 * Description:
 * A virtual pet project based on ESP32-C3 SuperMini,
 * SSD1306 OLED display, and TTP223 touch sensor.
 *
 * Development:
 * ChatGPT was used as an AI-assisted tool for code generation,
 * code explanation, feature development, and troubleshooting.
 * The project maintainer is responsible for project organization,
 * modifications, hardware integration, and testing actually performed.
 *
 * Third-party dependencies:
 * - Adafruit SSD1306
 * - Adafruit GFX Library
 *
 * See README.md and LICENSE for project details and licensing.
 */
/*
  Jie - ESP32-C3 SuperMini Desktop Pet
  Hardware:
    SSD1306 128x64 I2C OLED: SDA=GPIO6, SCL=GPIO7, address=0x3C
    TTP223 SIG=GPIO4, VCC=3V3, GND=GND
  Arduino IDE libraries:
    - Adafruit GFX Library
    - Adafruit SSD1306
  Board: ESP32C3 Dev Module

  Controls:
    HOME: short touch = pet Jie / random face; long hold (~1s) = menu
    MENU: short touch = next item; long hold (~1s) = select
    FEED: short touch = next food; hold ~1s = feed; hold ~2s = exit
    MESSAGE / PET_STATUS / FORTUNE: short touch advances; long hold returns home
    GAMES: touch down immediately jumps; hold ~1.5s exits (may jump once as hold begins)
  Storage namespace intentionally remains "doudou" to preserve existing saved data.
*/


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <esp_system.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C
#define TOUCH_PIN 4
#define SDA_PIN 6
#define SCL_PIN 7

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Preferences prefs;

const uint32_t DEBOUNCE_MS = 35;
const uint32_t LONG_MS = 1000;
const uint32_t VERY_LONG_MS = 2000;
const uint32_t GAME_EXIT_MS = 1500;
const uint32_t SAVE_INTERVAL_MS = 30000;
const uint32_t DECAY_INTERVAL_MS = 60000;

enum Screen {
  HOME, MENU, FEED, MESSAGE, PET_STATUS, GAME, DRESS, DAILY, FORTUNE, RESET_MENU, RESET_CONFIRM
};
Screen screen = HOME;

struct PetData {
  int level = 1;
  int exp = 0;
  int love = 20;
  int hunger = 75;  // 0 = hungry, 100 = full
  int energy = 80;
  int mood = 70;
  int best = 0;
  int face = 0;
  int lastSurpriseDay = -1;
  int lastFortuneDay = -1;
  int gamesPlayed = 0;
} pet;

// Main menu, including the pet status page.
const char* menuItems[] = {
  "Message", "Jie's Status", "Feed Jie", "Change Face",
  "Endless Run", "Fortune", "Reset Lv+EXP", "Back Home"
};
const int MENU_COUNT = sizeof(menuItems) / sizeof(menuItems[0]);
int menuIndex = 0;
int messageIndex = 0;
int foodIndex = 0;
int faceSelectIndex = 0;
int resetIndex = 0;
int resetConfirmIndex = 0;
int faceIndex = 0;
uint32_t lastTouchChange = 0;
uint32_t pressStart = 0;
uint32_t lastDecay = 0;
uint32_t lastSave = 0;
bool touchRaw = false, touchStable = false;
bool pressHandled = false;
bool homeLongHandled = false;
bool feedActionDone = false;
bool resetAll = false;
bool gameTouchWasDown = false;
uint32_t gamePressStart = 0;
bool gameJumpQueued = false;
bool dailyShowReward = false;
String transientMessage = "";
uint32_t transientUntil = 0;

const char* faces[] = {"Normal", "Happy", "Sad", "Angry", "Surprised", "Sleepy", "Wink"};
const int FACE_COUNT = 7;
uint32_t lastEyeFrame = 0;
uint8_t eyeAnimFrame = 0;
bool temporaryHappy = false;
uint32_t temporaryFaceUntil = 0;
const char* foods[] = {"Fish", "Milk", "Cookie", "Apple", "Berry"};
const int FOOD_COUNT = 5;
const char* fortunes[] = {
  "Lucky Day!", "Smile More!", "Good Things Ahead!",
  "You Are Loved!", "Keep Shining!", "Tiny Steps Count!"
};
const int FORTUNE_COUNT = 6;
const char* messages[] = {"Happy Birthday", "I Miss You", "Hope You Smile Every Day"};
const int MESSAGE_COUNT = 3;


int clamp100(int v) { return constrain(v, 0, 100); }

void saveData() {
  prefs.putInt("level", pet.level);
  prefs.putInt("exp", pet.exp);
  prefs.putInt("love", pet.love);
  prefs.putInt("hunger", pet.hunger);
  prefs.putInt("energy", pet.energy);
  prefs.putInt("mood", pet.mood);
  prefs.putInt("best", pet.best);
  prefs.putInt("face", pet.face);
  prefs.putInt("surpday", pet.lastSurpriseDay);
  prefs.putInt("fortday", pet.lastFortuneDay);
  prefs.putInt("games", pet.gamesPlayed);
  lastSave = millis();
}

void loadData() {
  pet.level = max(1, (int)prefs.getInt("level", 1));
  pet.exp = max(0, (int)prefs.getInt("exp", 0));
  pet.love = clamp100(prefs.getInt("love", 20));
  pet.hunger = clamp100(prefs.getInt("hunger", 75));
  pet.energy = clamp100(prefs.getInt("energy", 80));
  pet.mood = clamp100(prefs.getInt("mood", 70));
  pet.best = max(0, (int)prefs.getInt("best", 0));
  pet.face = constrain(prefs.getInt("face", 0), 0, FACE_COUNT - 1);
  pet.lastSurpriseDay = prefs.getInt("surpday", -1);
  pet.lastFortuneDay = prefs.getInt("fortday", -1);
  pet.gamesPlayed = max(0, (int)prefs.getInt("games", 0));
}

void addExp(int amount) {
  pet.exp += amount;
  while (pet.exp >= pet.level * 20) {
    pet.exp -= pet.level * 20;
    pet.level++;
    pet.love = clamp100(pet.love + 2);
    transientMessage = "LEVEL UP!";
    transientUntil = millis() + 1300;
  }
  saveData();
}

void petJie() {
  pet.love = clamp100(pet.love + 2);
  pet.mood = clamp100(pet.mood + 3);
  pet.energy = clamp100(pet.energy - 1);
  addExp(1);
  temporaryHappy = true;
  temporaryFaceUntil = millis() + 900;
  transientMessage = "Purr... Thank you!";
  transientUntil = millis() + 850;
}

void drawCentered(const String &s, int y, uint8_t size=1) {
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - (int)w) / 2, y);
  display.print(s);
}

void drawHeart(int x, int y, int scale=1) {
  display.fillCircle(x + 3*scale, y + 3*scale, 3*scale, SSD1306_WHITE);
  display.fillCircle(x + 9*scale, y + 3*scale, 3*scale, SSD1306_WHITE);
  display.fillTriangle(x, y + 4*scale, x + 12*scale, y + 4*scale,
                       x + 6*scale, y + 12*scale, SSD1306_WHITE);
}

void drawOpenCat(int cx, int cy, int face, int outfit) {
  // Open outline cat: ears and top/side cheek strokes, deliberately no closed face circle.
  display.drawLine(cx - 20, cy - 4, cx - 20, cy - 14, SSD1306_WHITE);
  display.drawLine(cx - 20, cy - 14, cx - 10, cy - 7, SSD1306_WHITE);
  display.drawLine(cx - 10, cy - 7, cx - 5, cy - 10, SSD1306_WHITE);
  display.drawLine(cx - 5, cy - 10, cx + 5, cy - 10, SSD1306_WHITE);
  display.drawLine(cx + 5, cy - 10, cx + 10, cy - 7, SSD1306_WHITE);
  display.drawLine(cx + 10, cy - 7, cx + 20, cy - 14, SSD1306_WHITE);
  display.drawLine(cx + 20, cy - 14, cx + 20, cy - 4, SSD1306_WHITE);
  // Side cheeks only; no closed jaw/face outline.
  display.drawLine(cx - 20, cy - 4, cx - 17, cy + 5, SSD1306_WHITE);
  display.drawLine(cx + 20, cy - 4, cx + 17, cy + 5, SSD1306_WHITE);

  if (face == 3) { // love eyes
    drawHeart(cx - 14, cy - 3, 1);
    drawHeart(cx + 3, cy - 3, 1);
  } else if (face == 4) { // sleepy
    display.drawLine(cx - 14, cy - 1, cx - 8, cy - 1, SSD1306_WHITE);
    display.drawLine(cx + 8, cy - 1, cx + 14, cy - 1, SSD1306_WHITE);
  } else if (face == 5) { // angry: eyes only, no eyebrows
    display.drawPixel(cx - 11, cy, SSD1306_WHITE);
    display.drawPixel(cx + 11, cy, SSD1306_WHITE);
  } else {
    if (face == 2) {
      display.fillCircle(cx - 11, cy - 1, 2, SSD1306_WHITE);
      display.fillCircle(cx + 11, cy - 1, 2, SSD1306_WHITE);
    } else {
      display.drawPixel(cx - 11, cy - 1, SSD1306_WHITE);
      display.drawPixel(cx + 11, cy - 1, SSD1306_WHITE);
    }
  }
  // Nose and mouth
  display.drawPixel(cx, cy + 3, SSD1306_WHITE);
  if (face == 5) {
    display.drawLine(cx - 4, cy + 8, cx, cy + 5, SSD1306_WHITE);
    display.drawLine(cx, cy + 5, cx + 4, cy + 8, SSD1306_WHITE);
  } else if (face == 4) {
    display.drawLine(cx - 3, cy + 7, cx + 3, cy + 7, SSD1306_WHITE);
  } else {
    display.drawLine(cx, cy + 4, cx, cy + 7, SSD1306_WHITE);
    display.drawLine(cx, cy + 7, cx - 4, cy + 9, SSD1306_WHITE);
    display.drawLine(cx, cy + 7, cx + 4, cy + 9, SSD1306_WHITE);
  }
  // Outfit accessories
  if (outfit == 1) { // crown
    display.drawLine(cx - 8, cy - 12, cx - 10, cy - 20, SSD1306_WHITE);
    display.drawLine(cx - 10, cy - 20, cx - 3, cy - 16, SSD1306_WHITE);
    display.drawLine(cx - 3, cy - 16, cx, cy - 22, SSD1306_WHITE);
    display.drawLine(cx, cy - 22, cx + 4, cy - 16, SSD1306_WHITE);
    display.drawLine(cx + 4, cy - 16, cx + 10, cy - 20, SSD1306_WHITE);
    display.drawLine(cx + 10, cy - 20, cx + 8, cy - 12, SSD1306_WHITE);
  } else if (outfit == 2) { // bow
    display.drawTriangle(cx - 5, cy - 12, cx - 12, cy - 17, cx - 12, cy - 9, SSD1306_WHITE);
    display.drawTriangle(cx + 5, cy - 12, cx + 12, cy - 17, cx + 12, cy - 9, SSD1306_WHITE);
    display.drawPixel(cx, cy - 12, SSD1306_WHITE);
  } else if (outfit == 3) { // hat
    display.drawLine(cx - 12, cy - 13, cx + 12, cy - 13, SSD1306_WHITE);
    display.drawLine(cx - 8, cy - 13, cx - 6, cy - 20, SSD1306_WHITE);
    display.drawLine(cx - 6, cy - 20, cx + 6, cy - 20, SSD1306_WHITE);
    display.drawLine(cx + 6, cy - 20, cx + 8, cy - 13, SSD1306_WHITE);
  } else if (outfit == 4) { // star
    display.drawLine(cx, cy - 22, cx, cy - 15, SSD1306_WHITE);
    display.drawLine(cx - 4, cy - 18, cx + 4, cy - 18, SSD1306_WHITE);
    display.drawLine(cx - 3, cy - 21, cx + 3, cy - 15, SSD1306_WHITE);
    display.drawLine(cx + 3, cy - 21, cx - 3, cy - 15, SSD1306_WHITE);
  }
}

void drawFoodIcon(int type, int cx, int cy) {
  switch(type) {
    case 0: // fish
      display.drawEllipse(cx, cy, 12, 6, SSD1306_WHITE);
      display.drawTriangle(cx-12, cy, cx-20, cy-6, cx-20, cy+6, SSD1306_WHITE);
      display.drawPixel(cx+6, cy-1, SSD1306_WHITE);
      display.drawLine(cx-3, cy-3, cx+1, cy, SSD1306_WHITE);
      break;
    case 1: // milk carton
      display.drawRect(cx-9, cy-10, 18, 22, SSD1306_WHITE);
      display.drawLine(cx-9, cy-10, cx-3, cy-15, SSD1306_WHITE);
      display.drawLine(cx-3, cy-15, cx+8, cy-10, SSD1306_WHITE);
      display.drawLine(cx-3, cy-15, cx-3, cy-10, SSD1306_WHITE);
      drawCentered("MILK", cy-2, 1);
      break;
    case 2: // cookie
      display.drawCircle(cx, cy, 11, SSD1306_WHITE);
      display.fillCircle(cx-4, cy-4, 1, SSD1306_WHITE);
      display.fillCircle(cx+4, cy-2, 1, SSD1306_WHITE);
      display.fillCircle(cx-1, cy+5, 1, SSD1306_WHITE);
      display.fillCircle(cx+5, cy+5, 1, SSD1306_WHITE);
      break;
    case 3: // apple
      display.drawCircle(cx, cy+1, 9, SSD1306_WHITE);
      display.drawLine(cx, cy-8, cx+2, cy-14, SSD1306_WHITE);
      display.drawLine(cx+2, cy-12, cx+9, cy-14, SSD1306_WHITE);
      break;
    default: // berry
      display.drawCircle(cx-3, cy+1, 7, SSD1306_WHITE);
      display.drawCircle(cx+4, cy+2, 7, SSD1306_WHITE);
      display.drawLine(cx, cy-5, cx+1, cy-12, SSD1306_WHITE);
      display.drawLine(cx+1, cy-10, cx+7, cy-12, SSD1306_WHITE);
      break;
  }
}

// OLED emotive eyes: no head outline, ears, nose, eyebrows or mouth.
// Each selected expression animates by changing eye shape/height over time.
void drawOLEDExpression(int expression, uint8_t frame, int centerY) {
  int eyeW = 25;
  int eyeH = 31;
  int gap = 18;
  int leftX = 64 - gap/2 - eyeW;
  int rightX = 64 + gap/2;
  int y = centerY - eyeH/2;
  int hL = eyeH, hR = eyeH;
  int yL = y, yR = y;
  int wL = eyeW, wR = eyeW;

  // Small periodic blink frame, without changing the selected expression.
  bool blink = (frame == 5 || frame == 6);
  switch (expression) {
    case 1: // Happy: clearly smiling, strongly curved closed eyes
      hL = hR = (frame % 6 < 3) ? 8 : 11;
      yL = yR = centerY - 2;
      wL = wR = 27;
      break;
    case 2: // Sad: eyes droop / become shorter
      hL = hR = (frame % 6 < 3) ? 23 : 19;
      yL = yR = centerY - hL/2 + 3;
      break;
    case 3: // Angry: sharp inward-tilting eyes, then add strong slanted brows below
      hL = hR = (frame % 5 < 2) ? 15 : 18;
      yL = yR = centerY - hL/2 + 2;
      wL = wR = 27;
      break;
    case 4: // Surprised: very round and large
      eyeW = 27; eyeH = 34; gap = 17;
      leftX = 64-gap/2-eyeW; rightX=64+gap/2;
      y = centerY-eyeH/2; hL=hR=eyeH; wL=wR=eyeW;
      if (frame % 8 == 0) { hL=hR=eyeH-3; yL=yR=y+1; }
      break;
    case 5: { // Sleep: rounded rectangles gradually flatten into a line
      uint8_t phase = frame % 10;
      hL = hR = (phase < 2) ? 20 : (phase < 4) ? 14 : (phase < 6) ? 8 : (phase < 8) ? 4 : 2;
      yL = yR = centerY - hL/2 + 5;
      break;
    }
    case 6: // Wink: one eye closes periodically
      hL = eyeH;
      hR = (frame % 8 < 4) ? 4 : eyeH;
      yR = centerY - hR/2;
      break;
    default: // Normal: big solid eyes with occasional blink
      break;
  }
  if (blink && expression == 0) {
    hL = hR = (frame == 5) ? 8 : 3;
    yL = yR = centerY - hL/2;
  }
  if (expression == 4) {
    display.fillRoundRect(leftX, yL == y ? y : yL, wL, hL, min(10, hL/2), SSD1306_WHITE);
    display.fillRoundRect(rightX, yR == y ? y : yR, wR, hR, min(10, hR/2), SSD1306_WHITE);
  } else {
    display.fillRoundRect(leftX, yL, wL, hL, min(9, hL/2), SSD1306_WHITE);
    display.fillRoundRect(rightX, yR, wR, hR, min(9, hR/2), SSD1306_WHITE);
  }
  if (expression == 3) {
    // Bold inward-slanting angry brows make the expression unmistakable.
    display.drawLine(leftX, centerY-13, leftX+eyeW-2, centerY-5, SSD1306_WHITE);
    display.drawLine(rightX+2, centerY-5, rightX+eyeW, centerY-13, SSD1306_WHITE);
  }
}

void drawHome() {
  display.clearDisplay();
  // Animate frames without delay; touch handling continues in loop().
  int activeFace = (temporaryHappy && millis() < temporaryFaceUntil) ? 1 : pet.face;
  drawOLEDExpression(activeFace, eyeAnimFrame, 31);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 1); display.print("Lv"); display.print(pet.level);
  String loveText = "Love:" + String(pet.love);
  display.setCursor(126 - (int)loveText.length() * 6, 1);
  display.print(loveText);
  if (millis() < transientUntil) drawCentered(transientMessage, 57, 1);
  else drawCentered("Tap: Jie Hold: menu", 57, 1);
  display.display();
}

void drawMenu() {
  display.clearDisplay();
  drawCentered("JIE MENU", 0, 1);
  int start = menuIndex - 1;
  if (start < 0) start = 0;
  if (start > MENU_COUNT - 4) start = MENU_COUNT - 4;
  for (int row=0; row<4; row++) {
    int idx = start + row;
    if (idx >= MENU_COUNT) break;
    int y = 13 + row*12;
    if (idx == menuIndex) {
      display.fillRect(0, y-1, 128, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, y);
    display.print(idx == menuIndex ? "> " : "  ");
    display.print(menuItems[idx]);
    display.setTextColor(SSD1306_WHITE);
  }
  display.display();
}

void drawFeed() {
  display.clearDisplay();
  drawCentered("FEED JIE", 0);
  drawFoodIcon(foodIndex, 64, 29);
  drawCentered(foods[foodIndex], 43);
  drawCentered("Tap: next  Hold: feed", 55);
  display.display();
}

void drawMessage() {
  display.clearDisplay();
  drawCentered("A LITTLE MESSAGE", 1);
  if (messageIndex == 0) drawCentered("Happy Birthday", 28, 1);
  else if (messageIndex == 1) drawCentered("I Miss You", 28, 1);
  else {
    drawCentered("Hope You Smile", 23, 1);
    drawCentered("Every Day", 35, 1);
  }
  drawCentered("Tap: next  Hold: home", 55);
  display.display();
}

void drawBar(int x, int y, int w, int value) {
  value = constrain(value, 0, 100);
  display.drawRect(x, y, w, 7, SSD1306_WHITE);
  int fillW = (w - 2) * value / 100;
  if (fillW > 0) display.fillRect(x + 1, y + 1, fillW, 5, SSD1306_WHITE);
}

void drawStatus() {
  display.clearDisplay();
  drawCentered("JIE STATUS", 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 12); display.print("Lv"); display.print(pet.level);
  display.print(" EXP "); display.print(pet.exp); display.print("/"); display.print(pet.level * 20);
  display.setCursor(2, 23); display.print("Love");   drawBar(38, 23, 86, pet.love);
  display.setCursor(2, 33); display.print("Food");   drawBar(38, 33, 86, pet.hunger);
  display.setCursor(2, 43); display.print("Energy"); drawBar(38, 43, 86, pet.energy);
  display.setCursor(2, 53); display.print("Mood");   drawBar(38, 53, 86, pet.mood);
  display.display();
}

void drawDress() {
  display.clearDisplay();
  drawCentered("CHANGE FACE", 0);
  drawOLEDExpression(faceSelectIndex, eyeAnimFrame, 28);
  drawCentered(String(faces[faceSelectIndex]), 47);
  drawCentered("Tap: next  Hold: save", 56);
  display.display();
}

void drawDaily() {
  display.clearDisplay();
  drawCentered("DAILY SURPRISE", 0);
  drawOLEDExpression(pet.face, eyeAnimFrame, 29);
  if (dailyShowReward) {
    drawCentered("Surprise unlocked!", 47);
    drawCentered("+5 EXP  +3 LOVE", 56);
  } else {
    drawCentered("Come back tomorrow!", 51);
  }
  display.display();
}

void drawFortune() {
  display.clearDisplay();
  drawCentered("FORTUNE", 0);
  drawOLEDExpression(pet.face, eyeAnimFrame, 25);
  String f = fortunes[faceIndex % FORTUNE_COUNT];
  drawCentered(f, 43);
  drawCentered("Tap: again  Hold: home", 55);
  display.display();
}

void drawResetMenu() {
  display.clearDisplay();
  drawCentered("RESET PROGRESS", 0);
  const char* opts[] = {"Level + EXP only", "Reset ALL data", "Cancel"};
  for (int i=0;i<3;i++) {
    int y=18+i*13;
    if (i==resetIndex) { display.fillRect(0,y-1,128,11,SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); }
    else display.setTextColor(SSD1306_WHITE);
    display.setCursor(5,y); display.print(i==resetIndex?"> ":"  "); display.print(opts[i]);
    display.setTextColor(SSD1306_WHITE);
  }
  display.display();
}

void drawResetConfirm() {
  display.clearDisplay();
  drawCentered("ARE YOU SURE?", 0);
  drawCentered(resetAll ? "Reset ALL data?" : "Clear level + EXP?", 18);
  drawCentered("YES", 39);
  drawCentered("Tap: Yes/No", 51);
  if (resetConfirmIndex == 1) {
    display.fillRect(32,37,64,12,SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK); drawCentered("NO", 39); display.setTextColor(SSD1306_WHITE);
  } else {
    display.fillRect(32,37,64,12,SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK); drawCentered("YES", 39); display.setTextColor(SSD1306_WHITE);
  }
  display.display();
}

// Game
int playerY = 48;
int playerVY = 0;
bool onGround = true;
int obstacleX = 128;
int obstacleW = 8;
int obstacleH = 13;
int gameScore = 0;
int gameSpeed = 3;
uint32_t lastGameFrame = 0;
bool gameOver = false;
bool endlessMode = false;
int runStreak = 0;

void resetGame(bool endless) {
  endlessMode = endless;
  playerY = 48; playerVY = 0; onGround = true;
  obstacleX = 128; obstacleW = 8; obstacleH = random(8,16);
  gameScore = 0; gameSpeed = 3; gameOver = false; runStreak = 0;
  lastGameFrame = millis();
  screen = GAME;
}

// Original requested running-cat sprite. Keep this drawing unchanged.
void drawPlayer(int x, int y) {
  display.drawLine(x, y + 5, x + 3, y, SSD1306_WHITE);
  display.drawLine(x + 3, y, x + 7, y + 4, SSD1306_WHITE);
  display.drawLine(x + 7, y + 4, x + 11, y, SSD1306_WHITE);
  display.drawLine(x + 11, y, x + 14, y + 5, SSD1306_WHITE);
  display.drawRoundRect(x + 1, y + 4, 13, 10, 3, SSD1306_WHITE);
  display.drawPixel(x + 5, y + 8, SSD1306_WHITE);
  display.drawPixel(x + 10, y + 8, SSD1306_WHITE);
  display.drawLine(x + 6, y + 11, x + 9, y + 11, SSD1306_WHITE);
}

void drawGame() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0); display.print("ENDLESS RUN");
  display.setCursor(91,0); display.print(gameScore);
  display.drawLine(0,59,127,59,SSD1306_WHITE);
  // Keep the originally requested cat sprite for the running game.
  drawPlayer(10, playerY - 3);
  // obstacle variants
  if ((gameScore / 5) % 3 == 0) {
    display.drawRect(obstacleX, 59-obstacleH, obstacleW, obstacleH, SSD1306_WHITE);
    display.drawLine(obstacleX+2, 59-obstacleH+3, obstacleX+obstacleW-2, 59-3, SSD1306_WHITE);
  } else if ((gameScore / 5) % 3 == 1) {
    display.drawTriangle(obstacleX,59,obstacleX+obstacleW/2,59-obstacleH,obstacleX+obstacleW,59,SSD1306_WHITE);
  } else {
    display.drawCircle(obstacleX+4,59-obstacleH/2,4,SSD1306_WHITE);
  }
  if (gameOver) {
    display.fillRect(20,18,88,30,SSD1306_BLACK);
    display.drawRect(20,18,88,30,SSD1306_WHITE);
    drawCentered("GAME OVER", 22);
    drawCentered("Score: " + String(gameScore), 33);
    drawCentered("Hold: home", 52);
  }
  display.display();
}

void gameJump() {
  if (onGround && !gameOver) {
    playerVY = -9;
    onGround = false;
  }
}

void updateGame() {
  uint32_t now = millis();
  if (gameOver) return;
  if (now - lastGameFrame < 38) return;
  lastGameFrame = now;

  playerVY += 1;
  playerY += playerVY;
  if (playerY >= 48) { playerY = 48; playerVY = 0; onGround = true; }

  obstacleX -= gameSpeed;
  if (obstacleX < -obstacleW) {
    obstacleX = 128 + random(0, 25);
    obstacleH = random(8, 17);
    gameScore++;
    runStreak++;
    if (endlessMode && gameScore % 5 == 0) gameSpeed = min(7, gameSpeed + 1);
    if (runStreak >= 5) { pet.love = clamp100(pet.love+1); runStreak=0; }
  }
  // Approximate collision box
  bool hitX = obstacleX < 25 && obstacleX + obstacleW > 10;
  bool hitY = playerY > (59 - obstacleH + 2);
  if (hitX && hitY) {
    gameOver = true;
    pet.gamesPlayed++;
    pet.energy = clamp100(pet.energy - 5);
    pet.mood = clamp100(pet.mood + (gameScore > 5 ? 4 : 1));
    if (gameScore > pet.best) pet.best = gameScore;
    addExp(gameScore / 3 + 1);
    saveData();
  }
}

void enterMenuItem() {
  switch(menuIndex) {
    case 0: screen=MESSAGE; messageIndex=0; break;
    case 1: screen=PET_STATUS; break;
    case 2: screen=FEED; foodIndex=0; feedActionDone=false; break;
    case 3: screen=DRESS; faceSelectIndex=pet.face; break;
    case 4: resetGame(true); break;
    case 5: screen=FORTUNE; faceIndex=random(0,FORTUNE_COUNT); break;
    case 6: resetAll=false; resetIndex=0; resetConfirmIndex=0; screen=RESET_CONFIRM; break;
    case 7: screen=HOME; break;
    default: screen=HOME; break;
  }
}

void performFeed() {
  if (feedActionDone) return;
  feedActionDone = true;
  int hungerGain[] = {24, 16, 12, 14, 18};
  int loveGain[] = {2, 1, 2, 2, 3};
  pet.hunger = clamp100(pet.hunger + hungerGain[foodIndex]);
  pet.love = clamp100(pet.love + loveGain[foodIndex]);
  pet.mood = clamp100(pet.mood + 3);
  pet.energy = clamp100(pet.energy + 1);
  addExp(2);
  temporaryHappy = true;
  temporaryFaceUntil = millis() + 1500;
  transientMessage = "Yummy! Thank you!";
  transientUntil = millis() + 1500;
  saveData();
  screen = HOME;
}

void performReset() {
  if (resetAll) {
    pet.level=1; pet.exp=0; pet.love=20; pet.hunger=75; pet.energy=80; pet.mood=70;
    pet.best=0; pet.face=0; pet.lastSurpriseDay=-1; pet.lastFortuneDay=-1; pet.gamesPlayed=0;
  } else {
    pet.level=1; pet.exp=0;
  }
  saveData();
  screen=HOME;
  transientMessage = resetAll ? "All data reset" : "Level + EXP cleared";
  transientUntil=millis()+1500;
}

void handleShortPress() {
  switch(screen) {
    case HOME: petJie(); break;
    case MENU: menuIndex=(menuIndex+1)%MENU_COUNT; break;
    case FEED: foodIndex=(foodIndex+1)%FOOD_COUNT; feedActionDone=false; break;
    case MESSAGE: messageIndex=(messageIndex+1)%MESSAGE_COUNT; break;
    case PET_STATUS: screen=HOME; break;
    case DRESS: faceSelectIndex=(faceSelectIndex+1)%FACE_COUNT; break;
    case DAILY: screen=HOME; break;
    case FORTUNE: faceIndex=random(0,FORTUNE_COUNT); break;
    case RESET_MENU: resetIndex=(resetIndex+1)%3; break;
    case RESET_CONFIRM: resetConfirmIndex=1-resetConfirmIndex; break;
    case GAME:
      if (gameOver) screen=HOME;
      else gameJump();
      break;
  }
}

void handleLongPress() {
  switch(screen) {
    case HOME: screen=MENU; break;
    case MENU: enterMenuItem(); break;
    case FEED: performFeed(); break;
    case MESSAGE: case PET_STATUS: case DAILY: case FORTUNE: screen=HOME; break;
    case DRESS:
      pet.face = faceSelectIndex;
      saveData();
      screen = HOME;
      temporaryHappy = false;
      transientMessage = "Face saved!";
      transientUntil = millis() + 1200;
      break;
    case RESET_MENU:
      if (resetIndex==2) screen=HOME;
      else {
        resetAll = (resetIndex==1);
        resetConfirmIndex=0;
        screen=RESET_CONFIRM;
      }
      break;
    case RESET_CONFIRM:
      if (resetConfirmIndex==0) performReset();
      else screen=RESET_MENU;
      break;
    case GAME:
      if (gameOver) screen=HOME;
      else screen=HOME;
      break;
  }
}

void drawCurrentScreen() {
  switch(screen) {
    case HOME: drawHome(); break;
    case MENU: drawMenu(); break;
    case FEED: drawFeed(); break;
    case MESSAGE: drawMessage(); break;
    case PET_STATUS: drawStatus(); break;
    case GAME: drawGame(); break;
    case DRESS: drawDress(); break;
    case DAILY: drawDaily(); break;
    case FORTUNE: drawFortune(); break;
    case RESET_MENU: drawResetMenu(); break;
    case RESET_CONFIRM: drawResetConfirm(); break;
  }
}

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed. Check wiring/address.");
    while(true) delay(100);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  prefs.begin("doudou", false); // Keep existing namespace for saved data compatibility
  loadData();
  randomSeed((uint32_t)esp_random());
  lastDecay = millis();
  lastSave = millis();
  drawCentered("Hello, I'm Jie!", 24, 1);
  display.display();
  delay(900);
}

void loop() {
  uint32_t now = millis();
  bool reading = digitalRead(TOUCH_PIN) == HIGH;

  if (reading != touchRaw) {
    touchRaw = reading;
    lastTouchChange = now;
  }
  if ((now - lastTouchChange) > DEBOUNCE_MS && touchStable != touchRaw) {
    touchStable = touchRaw;
    if (touchStable) {
      pressStart = now;
      pressHandled = false;
      homeLongHandled = false;
      feedActionDone = false;
      if (screen == GAME) {
        gameTouchWasDown = true;
        gamePressStart = now;
        if (!gameOver) gameJump(); // Immediate jump on touch-down
      }
    } else {
      if (screen != GAME && !pressHandled) {
        uint32_t held = now - pressStart;
        if (screen == FEED) {
          // Defer the feed action until release so a 2-second hold can exit instead.
          if (held >= VERY_LONG_MS) screen = HOME;
          else if (held >= LONG_MS) performFeed();
          else handleShortPress();
        } else if (held >= LONG_MS) handleLongPress();
        else handleShortPress();
      }
      if (screen == GAME) gameTouchWasDown = false;
      pressHandled = false;
    }
  }

  if (touchStable && !pressHandled && screen != GAME) {
    uint32_t held = now - pressStart;
    if (screen == HOME && held >= LONG_MS) {
      screen = MENU; pressHandled = true;
    } else if (screen == MENU && held >= LONG_MS) {
      enterMenuItem(); pressHandled = true;
    } else if (screen != HOME && screen != MENU && screen != FEED && held >= LONG_MS) {
      // Keep confirm/return actions on release to avoid accidental repeated triggers.
      // RESET screens also use release-based long press.
    }
  }

  if (screen == GAME && touchStable && gameTouchWasDown && now - gamePressStart >= GAME_EXIT_MS) {
    screen = HOME;
    gameTouchWasDown = false;
  }

  if (now - lastEyeFrame >= 180) {
    lastEyeFrame = now;
    eyeAnimFrame = (eyeAnimFrame + 1) % 8;
  }
  if (temporaryHappy && now >= temporaryFaceUntil) temporaryHappy = false;

  if (now - lastDecay >= DECAY_INTERVAL_MS) {
    lastDecay = now;
    pet.hunger = clamp100(pet.hunger - 1);
    pet.energy = clamp100(pet.energy - (screen == HOME ? 1 : 2));
    if (pet.hunger < 25) pet.mood = clamp100(pet.mood - 1);
    if (screen == HOME && pet.energy < 15) pet.face = 5;
  }
  if (now - lastSave >= SAVE_INTERVAL_MS) saveData();

  if (screen == GAME) updateGame();
  drawCurrentScreen();
  delay(12);
}
