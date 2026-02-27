#include <M5Stack.h>

// Simple key=value;key2=value2;... protocol over Serial.
// Example line from host:
// time=2026-02-27 13:45:12;hostname=cyberdeck;cpu=12.3;ram_used_mb=1024;ram_total_mb=3950;ram_percent=25.9;load_1=0.21;load_5=0.17;load_15=0.11

// Parsed stats
struct Stats {
    String time;
    String hostname;
    float cpu = 0.0f;
    int ramUsedMb = 0;
    int ramTotalMb = 0;
    float ramPercent = 0.0f;
    float load1 = 0.0f;
    float load5 = 0.0f;
    float load15 = 0.0f;
};

static Stats g_stats;

// Serial line buffer
static String g_lineBuffer;

// For a little cyberdeck flair: animate CPU and RAM bar "scan lines"
static uint32_t g_lastRedrawMs = 0;
static uint8_t g_animPhase = 0;

// Utility: split "key=value" into key + value
static bool splitKeyValue(const String &kv, String &key, String &value) {
    int idx = kv.indexOf('=');
    if (idx <= 0) {
        return false;
    }
    key = kv.substring(0, idx);
    value = kv.substring(idx + 1);
    key.trim();
    value.trim();
    return key.length() > 0;
}

static float toFloat(const String &s) {
    return s.toFloat();
}

static int toInt(const String &s) {
    return s.toInt();
}

// Parse a full line of "key=value;key2=value2;..."
static void parseStatsLine(const String &line) {
    Stats next = g_stats;  // start from previous; update fields we see

    int start = 0;
    while (start < line.length()) {
        int end = line.indexOf(';', start);
        if (end < 0) {
            end = line.length();
        }
        String token = line.substring(start, end);
        token.trim();
        if (token.length() > 0) {
            String key, value;
            if (splitKeyValue(token, key, value)) {
                if (key == "time") {
                    next.time = value;
                } else if (key == "hostname") {
                    next.hostname = value;
                } else if (key == "cpu") {
                    next.cpu = toFloat(value);
                } else if (key == "ram_used_mb") {
                    next.ramUsedMb = toInt(value);
                } else if (key == "ram_total_mb") {
                    next.ramTotalMb = toInt(value);
                } else if (key == "ram_percent") {
                    next.ramPercent = toFloat(value);
                } else if (key == "load_1") {
                    next.load1 = toFloat(value);
                } else if (key == "load_5") {
                    next.load5 = toFloat(value);
                } else if (key == "load_15") {
                    next.load15 = toFloat(value);
                }
            }
        }
        start = end + 1;
    }

    g_stats = next;
}

// Draw a horizontal bar with outline and animated scanline
static void drawBar(int x, int y, int w, int h, float percent, uint16_t baseColor) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    // Border
    M5.Lcd.drawRect(x, y, w, h, baseColor);

    // Fill amount
    int innerW = w - 2;
    int innerH = h - 2;
    int filled = (int)((percent / 100.0f) * innerW);

    // Background inside bar
    M5.Lcd.fillRect(x + 1, y + 1, innerW, innerH, TFT_BLACK);

    if (filled > 0) {
        // Main bar
        M5.Lcd.fillRect(x + 1, y + 1, filled, innerH, baseColor);

        // Scanline overlay for cyber look
        uint16_t scanColor = (g_animPhase % 4 < 2) ? TFT_BLACK : TFT_DARKGREEN;
        for (int yy = y + 1; yy < y + 1 + innerH; yy += 2) {
            M5.Lcd.drawFastHLine(x + 1, yy, filled, scanColor);
        }
    }
}

static void drawStaticFrame() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextFont(1);

    // Header line
    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Lcd.setCursor(4, 4);
    M5.Lcd.print("> CORE-LINK//STATUS");

    // Thin horizontal divider
    M5.Lcd.drawFastHLine(0, 14, 320, TFT_DARKGREY);

    // Section labels
    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Lcd.setCursor(4, 20);
    M5.Lcd.print("CPU");

    M5.Lcd.setCursor(4, 50);
    M5.Lcd.print("RAM");

    M5.Lcd.setCursor(4, 90);
    M5.Lcd.print("LOAD");

    M5.Lcd.setCursor(4, 130);
    M5.Lcd.print("TIME");

    // Small corner decorations
    M5.Lcd.drawRect(0, 0, 320, 240, TFT_DARKGREY);
    M5.Lcd.drawRect(2, 2, 316, 236, TFT_DARKGREY);
}

static void drawDynamicStats() {
    // CPU bar + text
    int barX = 60;
    int barY = 18;
    int barW = 240;
    int barH = 16;
    drawBar(barX, barY, barW, barH, g_stats.cpu, TFT_GREEN);

    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(barX + 2, barY + 2);
    M5.Lcd.printf("CPU: %5.1f%%", g_stats.cpu);

    // RAM bar + text
    int ramY = 48;
    drawBar(barX, ramY, barW, barH, g_stats.ramPercent, TFT_YELLOW);
    M5.Lcd.setCursor(barX + 2, ramY + 2);
    if (g_stats.ramTotalMb > 0) {
        M5.Lcd.printf("RAM: %4d/%4d MB (%4.1f%%)",
                      g_stats.ramUsedMb,
                      g_stats.ramTotalMb,
                      g_stats.ramPercent);
    } else {
        M5.Lcd.printf("RAM: %4d MB (%4.1f%%)", g_stats.ramUsedMb, g_stats.ramPercent);
    }

    // Load averages
    M5.Lcd.setTextColor(TFT_DARKCYAN, TFT_BLACK);
    M5.Lcd.setCursor(60, 88);
    M5.Lcd.printf("L1: %4.2f  L5: %4.2f  L15: %4.2f",
                  g_stats.load1, g_stats.load5, g_stats.load15);

    // Hostname + time
    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Lcd.setCursor(60, 128);
    if (g_stats.hostname.length() > 0) {
        M5.Lcd.printf("[%s]", g_stats.hostname.c_str());
    } else {
        M5.Lcd.print("[no-host]");
    }

    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(60, 144);
    if (g_stats.time.length() > 0) {
        M5.Lcd.print(g_stats.time);
    } else {
        M5.Lcd.print("waiting for link...");
    }

    // Footer "signal" indicator that ticks with updates
    M5.Lcd.setCursor(4, 220);
    M5.Lcd.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    M5.Lcd.printf("SERIAL-LINK: %s  PHASE:%02u",
                  (g_stats.time.length() > 0 ? "ONLINE" : "IDLE"),
                  g_animPhase);
}

static void processSerialInput() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            String line = g_lineBuffer;
            g_lineBuffer = "";
            line.trim();
            if (line.length() > 0) {
                parseStatsLine(line);
            }
        } else {
            if (g_lineBuffer.length() < 512) {
                g_lineBuffer += c;
            } else {
                // Overflow guard: reset buffer
                g_lineBuffer = "";
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    M5.begin(true, false, true, false);
    M5.Power.setPowerWLEDSet(false);

    M5.Lcd.setRotation(1);  // wide layout
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextSize(1);

    drawStaticFrame();

    Serial.println("=== M5Core1 Cyberdeck Status Display ===");
    Serial.println("Waiting for serial stats lines from host...");
}

void loop() {
    M5.update();

    processSerialInput();

    uint32_t now = millis();
    if (now - g_lastRedrawMs > 200) {
        g_lastRedrawMs = now;
        g_animPhase++;
        drawDynamicStats();
    }

    delay(5);
}

