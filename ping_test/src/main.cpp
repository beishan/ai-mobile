#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

// OSPTek EPD0426A02: 800 x 480, SSD1677.
// Wiring: CS=5, DC=26, RST=27, BUSY=4, SCK=18, MOSI=23.
GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT> epd(
    GxEPD2_426_GDEQ0426T82(5, 26, 27, 4));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

void drawHeader() {
  epd.fillRect(0, 0, 480, 72, GxEPD_BLACK);
  u8g2Fonts.setForegroundColor(GxEPD_WHITE);
  u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2Fonts.drawUTF8(24, 30, "鱼鹰光电 4.26 寸电子纸测试");
  u8g2Fonts.drawUTF8(24, 58, "OSPTek EPD 480x800  SSD1677  SPI");
}

void drawChineseEnglishTest() {
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

  epd.drawRect(18, 92, 444, 228, GxEPD_BLACK);
  epd.drawFastHLine(18, 144, 444, GxEPD_BLACK);
  epd.drawFastVLine(240, 92, 228, GxEPD_BLACK);

  u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2Fonts.drawUTF8(34, 126, "中文显示");
  u8g2Fonts.drawUTF8(258, 126, "English");

  u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312a);
  u8g2Fonts.drawUTF8(34, 176, "黑白双色墨水屏");
  u8g2Fonts.drawUTF8(34, 206, "分辨率: 480 x 800");
  u8g2Fonts.drawUTF8(34, 236, "接口: SPI");
  u8g2Fonts.drawUTF8(34, 266, "驱动: SSD1677");
  u8g2Fonts.drawUTF8(34, 296, "测试: 汉字 标点 123");

  epd.setTextColor(GxEPD_BLACK);
  epd.setTextSize(2);
  epd.setCursor(258, 172);
  epd.print("Hello ESP32");
  epd.setCursor(258, 202);
  epd.print("Black / White");
  epd.setCursor(258, 232);
  epd.print("SPI E-Paper");
  epd.setCursor(258, 262);
  epd.print("SSD1677");
}

void drawPatterns() {
  epd.drawRect(18, 348, 444, 174, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2Fonts.drawUTF8(34, 382, "线条与填充测试");

  for (int i = 0; i < 8; ++i) {
    const int x = 38 + i * 52;
    if (i & 1) {
      epd.fillRect(x, 410, 38, 72, GxEPD_BLACK);
    } else {
      epd.drawRect(x, 410, 38, 72, GxEPD_BLACK);
      for (int yy = 412; yy < 480; yy += 4) {
        epd.drawFastHLine(x + 2, yy, 34, GxEPD_BLACK);
      }
    }
  }

  for (int r = 8; r <= 54; r += 8) {
    epd.drawCircle(388, 448, r, GxEPD_BLACK);
  }
}

void drawInfoPanel() {
  epd.drawRect(18, 550, 444, 212, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2Fonts.drawUTF8(34, 586, "刷新状态");

  u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312a);
  u8g2Fonts.drawUTF8(34, 626, "1. 初始化 SSD1677");
  u8g2Fonts.drawUTF8(34, 656, "2. 写入黑白帧缓存");
  u8g2Fonts.drawUTF8(34, 686, "3. 触发全屏刷新后休眠");

  epd.setTextColor(GxEPD_BLACK);
  epd.setTextSize(2);
  epd.setCursor(34, 742);
  epd.print("Build: ");
  epd.print(__DATE__);
  epd.print(" ");
  epd.print(__TIME__);
}

void drawDemoPage() {
  epd.fillScreen(GxEPD_WHITE);
  drawHeader();
  drawChineseEnglishTest();
  drawPatterns();
  drawInfoPanel();
}

void drawUiScene() {
  epd.fillScreen(GxEPD_WHITE);

  epd.fillRect(0, 0, 480, 64, GxEPD_BLACK);
  u8g2Fonts.setForegroundColor(GxEPD_WHITE);
  u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2Fonts.drawUTF8(22, 40, "电子纸界面组件测试");

  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2Fonts.drawUTF8(24, 104, "书架");

  const char* titles[] = {"长篇小说", "技术笔记", "旅行随笔"};
  const int progress[] = {68, 35, 92};
  for (int i = 0; i < 3; ++i) {
    const int y = 128 + i * 154;
    epd.drawRoundRect(20, y, 440, 126, 8, GxEPD_BLACK);
    epd.fillRect(38, y + 18, 72, 90, i == 1 ? GxEPD_BLACK : GxEPD_WHITE);
    epd.drawRect(38, y + 18, 72, 90, GxEPD_BLACK);
    if (i != 1) {
      epd.drawLine(48, y + 34, 100, y + 34, GxEPD_BLACK);
      epd.drawLine(48, y + 48, 94, y + 48, GxEPD_BLACK);
    }
    u8g2Fonts.drawUTF8(132, y + 42, titles[i]);
    epd.drawRect(132, y + 70, 280, 16, GxEPD_BLACK);
    epd.fillRect(135, y + 73, (274 * progress[i]) / 100, 10, GxEPD_BLACK);
    epd.setTextColor(GxEPD_BLACK);
    epd.setTextSize(1);
    epd.setCursor(132, y + 106);
    epd.print("Reading progress: ");
    epd.print(progress[i]);
    epd.print('%');
  }

  epd.drawRoundRect(20, 620, 210, 72, 8, GxEPD_BLACK);
  epd.fillRoundRect(250, 620, 210, 72, 8, GxEPD_BLACK);
  u8g2Fonts.drawUTF8(78, 663, "目录");
  u8g2Fonts.setForegroundColor(GxEPD_WHITE);
  u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
  u8g2Fonts.drawUTF8(308, 663, "继续阅读");

  epd.setTextColor(GxEPD_BLACK);
  epd.setTextSize(1);
  epd.setCursor(24, 752);
  epd.print("UI TEST: cards / progress / buttons / inverse color");
}

void drawNovelPage() {
  epd.fillScreen(GxEPD_WHITE);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

  u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2Fonts.drawUTF8(24, 40, "第十二章  雨夜来客");
  epd.drawFastHLine(20, 58, 440, GxEPD_BLACK);

  u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312a);
  const char* lines[] = {
      "窗外的雨从黄昏一直下到深夜。", "檐角的水珠落在青石板上，声音清亮。",
      "林舟合上手中的书，抬头望向门外。", "远处有一盏灯，正沿着山路缓缓而来。",
      "起初只是微弱的一点，像落在夜色里的星。", "片刻之后，脚步声穿过竹林，越来越近。",
      "他把炉火拨亮，又添了一壶新茶。", "门环轻响三声，来人却没有立刻开口。",
      "谁在外面？林舟问道。", "雨声忽然变大，掩去了门外低低的回答。",
      "他起身推门，冷风裹着水汽涌进屋内。", "台阶下站着一位披蓑衣的年轻人。",
      "那人抬起头，递来一封已经湿透的信。", "信封上只有四个字：故人亲启。",
      "林舟看了很久，才侧身让出门口。", "这一夜，山中的灯直到天明也没有熄灭。"};

  for (int i = 0; i < 16; ++i) {
    u8g2Fonts.drawUTF8(28, 96 + i * 36, lines[i]);
  }

  epd.drawFastHLine(20, 690, 440, GxEPD_BLACK);
  epd.setTextColor(GxEPD_BLACK);
  epd.setTextSize(1);
  epd.setCursor(24, 716);
  epd.print("EPD Reader   Font 12px   WiFi OFF");
  epd.setCursor(192, 752);
  epd.print("- 128 / 356 -");
}

void drawPartialStatus(uint8_t step) {
  // This function draws only inside the partial window at the bottom.
  epd.fillRect(16, 700, 448, 84, GxEPD_WHITE);
  epd.drawRoundRect(16, 700, 448, 84, 8, GxEPD_BLACK);

  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312a);
  u8g2Fonts.drawUTF8(32, 730, "局部刷新测试");

  epd.setTextColor(GxEPD_BLACK);
  epd.setTextSize(2);
  epd.setCursor(32, 754);
  epd.print("COUNT ");
  epd.print(step);
  epd.print(" / 10");

  for (int i = 0; i < 10; ++i) {
    const int x = 250 + i * 18;
    if (i < step) epd.fillRect(x, 738, 13, 22, GxEPD_BLACK);
    else epd.drawRect(x, 738, 13, 22, GxEPD_BLACK);
  }
}

void fullRefresh(void (*drawScene)(), const __FlashStringHelper* label) {
  Serial.print(F("Full refresh: "));
  Serial.println(label);
  epd.setFullWindow();
  epd.firstPage();
  do {
    drawScene();
  } while (epd.nextPage());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("ESP32 + OSPTek 4.26 inch EPD SSD1677 test"));
  Serial.println(F("Pins: SCK=18 MOSI=23 CS=5 DC=26 RST=27 BUSY=4"));

  // GxEPD2 uses ESP32 VSPI defaults: SCK=18 and MOSI=23.
  epd.init(115200, true, 20, false);
  epd.setRotation(1);
  epd.setFullWindow();

  Serial.print(F("EPD native="));
  Serial.print(GxEPD2_426_GDEQ0426T82::WIDTH);
  Serial.print('x');
  Serial.print(GxEPD2_426_GDEQ0426T82::HEIGHT);
  Serial.print(F(" logical="));
  Serial.print(epd.width());
  Serial.print('x');
  Serial.println(epd.height());

  u8g2Fonts.begin(epd);
  u8g2Fonts.setFontMode(1);
  u8g2Fonts.setFontDirection(0);

  Serial.println(F("EPD multi-scene test begin"));

  fullRefresh(drawDemoPage, F("graphics and text"));
  delay(3500);

  fullRefresh(drawUiScene, F("reader UI components"));
  delay(3500);

  fullRefresh(drawNovelPage, F("novel reading page"));
  delay(2500);

  Serial.println(F("Partial refresh test: 10 updates"));
  epd.setPartialWindow(16, 700, 448, 84);
  for (uint8_t step = 1; step <= 10; ++step) {
    epd.firstPage();
    do {
      drawPartialStatus(step);
    } while (epd.nextPage());
    Serial.print(F("Partial update "));
    Serial.println(step);
    delay(800);
  }

  epd.hibernate();
  Serial.println(F("All EPD scenarios finished and display hibernated"));
}

void loop() {
  delay(1000);
}
