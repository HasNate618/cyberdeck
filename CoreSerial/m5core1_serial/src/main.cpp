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
    String localIp;
    String publicIp;
    float cpuTempC = 0.0f;
    float netUpMbps = 0.0f;
    float netDownMbps = 0.0f;
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
                } else if (key == "local_ip") {
                    next.localIp = value;
                } else if (key == "public_ip") {
                    next.publicIp = value;
                } else if (key == "cpu_temp_c") {
                    next.cpuTempC = toFloat(value);
                } else if (key == "net_up_mbps") {
                    next.netUpMbps = toFloat(value);
                } else if (key == "net_down_mbps") {
                    next.netDownMbps = toFloat(value);
                }
            }
        }
        start = end + 1;
    }

    g_stats = next;
}

// Draw a horizontal bar with outline
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
    }
}

static void drawStaticFrame() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextFont(1);

    // Frame
    M5.Lcd.drawRect(0, 0, 320, 240, TFT_DARKGREY);
    M5.Lcd.drawRect(2, 2, 316, 236, TFT_DARKGREY);

    // Header divider (hostname will be drawn dynamically above this)
    M5.Lcd.drawFastHLine(0, 28, 320, TFT_DARKGREY);

    // Section labels (all text size 2), spaced evenly down the panel
    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    // TIME near top of content area
    M5.Lcd.setCursor(4, 40);
    M5.Lcd.print("TIME");

    // IP section below TIME
    M5.Lcd.setCursor(4, 82);
    M5.Lcd.print("IP");

    // CPU roughly mid-lower
    M5.Lcd.setCursor(4, 134);
    M5.Lcd.print("CPU");

    // RAM near bottom
    M5.Lcd.setCursor(4, 186);
    M5.Lcd.print("RAM");
}

static void drawDynamicStats() {
    // Header with hostname
    // Clear inside the frame only so we don't erase borders
    M5.Lcd.fillRect(3, 3, 314, 22, TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Lcd.setCursor(4, 4);
    if (g_stats.hostname.length() > 0) {
        M5.Lcd.printf("[%s]", g_stats.hostname.c_str());
    } else {
        M5.Lcd.print("[no-host]");
    }
    M5.Lcd.print(" //STATUS");

    // Common text settings for all stats
    M5.Lcd.setTextSize(2);

    // TIME row (evenly spaced beneath header)
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.fillRect(72, 32, 244, 24, TFT_BLACK);
    M5.Lcd.setCursor(72, 36);
    if (g_stats.time.length() > 0) {
        M5.Lcd.print(g_stats.time);
    } else {
        M5.Lcd.print("waiting...");
    }

    // IP row
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.fillRect(72, 74, 244, 32, TFT_BLACK);
    M5.Lcd.setCursor(72, 74);
    if (g_stats.localIp.length() > 0) {
        M5.Lcd.printf("LAN %s", g_stats.localIp.c_str());
    } else {
        M5.Lcd.print("LAN n/a");
    }
    M5.Lcd.setCursor(72, 90);
    if (g_stats.publicIp.length() > 0) {
        M5.Lcd.printf("WAN %s", g_stats.publicIp.c_str());
    } else {
        M5.Lcd.print("WAN n/a");
    }

    // CPU bar + percentage to the right (spaced into lower half)
    int barX = 72;
    int barY = 128;
    int barW = 170;
    int barH = 18;
    drawBar(barX, barY, barW, barH, g_stats.cpu, TFT_PURPLE);

    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(barX + barW + 8, barY);
    M5.Lcd.printf("%4.1f%%", g_stats.cpu);

    // RAM bar + percentage to the right, near bottom
    int ramY = 180;
    drawBar(barX, ramY, barW, barH, g_stats.ramPercent, TFT_RED);

    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(barX + barW + 8, ramY);
    if (g_stats.ramTotalMb > 0) {
        M5.Lcd.printf("%4.1f%%", g_stats.ramPercent);
    } else {
        M5.Lcd.print("--.-%");
    }
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
    M5.Lcd.setTextSize(2);

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

