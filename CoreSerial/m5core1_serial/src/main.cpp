#include <M5Stack.h>
#include <string.h>

// Simple key=value;key2=value2;... protocol over Serial.
// Example line from host:
// time=2026-02-27 13:45:12;hostname=cyberdeck;cpu=12.3;ram_used_mb=1024;ram_total_mb=3950;ram_percent=25.9;

// Parsed stats
struct Stats {
    String time;
    String user;
    String hostname;
    float cpu = 0.0f;
    int ramUsedMb = 0;
    int ramTotalMb = 0;
    float ramPercent = 0.0f;
    String localIp;
    String publicIp;
    float cpuTempC = 0.0f;
    float netUpMbps = 0.0f;
    float netDownMbps = 0.0f;
};

static Stats g_stats;

// Serial line buffer
static String g_lineBuffer;

// Dashboard redraw timing
static uint32_t g_lastRedrawMs = 0;
static uint8_t g_animPhase      = 0;

// Cached values for header/IP so we only redraw when they change
static String g_prevUser;
static String g_prevHostname;
static String g_prevLocalIp;
static String g_prevPublicIp;
static bool   g_headerIpInitialized = false;

// Display modes
enum DisplayMode {
    MODE_DASHBOARD = 0,
    MODE_MATRIX    = 1,
    MODE_ART       = 2,
};

static DisplayMode g_mode = MODE_DASHBOARD;

// ============================================================================
// ASCII art mode state (BtnB)
// ============================================================================

static const char *const ART_ARCHLOGO[] = {
    "                   -`",
    "                  .o+`",
    "                  .o+`",
    "                 `ooo/",
    "                `+oooo:",
    "                `+oooo:",
    "               `+oooooo:",
    "               -+oooooo+:",
    "               -+oooooo+:",
    "             `/:-:++oooo+:",
    "            `/++++/+++++++:",
    "            `/++++/+++++++:",
    "           `/++++++++++++++:",
    "          `/+++ooooooooooooo/`",
    "          `/+++ooooooooooooo/`",
    "         ./ooosssso++osssssso+`",
    "        .oossssso-````/ossssss+`",
    "        .oossssso-````/ossssss+`",
    "       -osssssso.      :ssssssso.",
    "      :osssssss/        osssso+++.",
    "      :osssssss/        osssso+++.",
    "     /ossssssss/        +ssssooo/-",
    "   `/ossssso+/:-        -:/+osssso+-",
    "   `/ossssso+/:-        -:/+osssso+-",
    "  `+sso+:-`                 `.-/+oso:",
    " `++:.                           `-/+/",
    " `++:.                           `-/+/",
    " .`                                 `",
};

static const char *const ART_CYBRDECK[] = {
"     █████████  ██   ▄▄  ▀█████████▄     ▄███████  ",
"    ███    ███ ███   ██▄   ███    ███   ███    ███ ",
"    ███    ███ ███   ██▄   ███    ███   ███    ███ ",
"    ███    █▀  ███▄▄▄███   ███    ███   ███    ███ ",
"    ███           ▀▀▀███  ▄███▄▄▄██▀   ▄███▄▄▄▄██  ",
"    ███        ▄██   ███  ▀███▀▀▀██▄   ▀███▀▀▀   ",
"    ███    █▄  ███   ███   ███    ██▄ ▀███████████ ",
"    ███    ███ ███   ███   ███    ██▄   ███    ███ ",
"    ███    ███ ███   ███   ███    ██    ███    ███ ",
"    ████████▀   ▀█████▀  ▄█████████     ███    ███ ",
"                                        ███    ███ ",
"                                                   ",
"    ████████▄     ▄████████  ▄████████    ▄█   ▄█         ",
"    ███   ▀███   ███    ███ ███    ███   ███  ███        ",
"    ███    ███   ███    ███ ███    ███   ███ ▄███        ",
"    ███    ███   ███    █▀  ███    █▀    ███▐██▀          ",
"    ███    ███  ▄███▄▄▄     ███         ▄█████▀           ",
"    ███    ███ ▀▀███▀▀▀     ███         ▀█████▄           ",
"    ███    ███   ███    █▄  ███    █▄    ███ ██▄          ",
"    ███    ███   ███    ███ ███    ███   ███ ▀███▄        ",
"    ███   ▄███   ███    ███ ███    ███   ███  ███▄        ",
"    ████████▀    ██████████ ████████▀    ███   ▀█         ",
"                                         ▀▀        ",
};

static const char *const *const g_asciiArts[]      = {ART_ARCHLOGO, ART_CYBRDECK};
static const int                 g_asciiArtLines[] = {
    (int)(sizeof(ART_ARCHLOGO) / sizeof(ART_ARCHLOGO[0])),
    (int)(sizeof(ART_CYBRDECK) / sizeof(ART_CYBRDECK[0])),
};
static const int g_numAsciiArts = (int)(sizeof(g_asciiArts) / sizeof(g_asciiArts[0]));
static int       g_currentArtIndex = 0;

// ============================================================================
// Matrix rain mode state (BtnC)
// ============================================================================

static const int MATRIX_TEXT_HEIGHT     = 8;
static const int MATRIX_COLS            = 40;  // ~320 / 8
static const int MATRIX_SCREEN_H        = 240;
static const int MATRIX_TRAIL_MIN_ROWS  = 4;   // min trail length (in rows)
static const int MATRIX_TRAIL_MAX_ROWS  = 12;  // max trail length (in rows)

static int  g_matrixDropY[MATRIX_COLS];
static int  g_matrixTrailLen[MATRIX_COLS];
static bool g_matrixInitialized = false;

// Render the currently selected ASCII art centered on screen
static void renderCurrentArt() {
    if (g_numAsciiArts <= 0) {
        return;
    }

    int idx = g_currentArtIndex % g_numAsciiArts;
    if (idx < 0) idx = 0;

    const char *const *lines = g_asciiArts[idx];
    int lineCount            = g_asciiArtLines[idx];

    // Character metrics for font 1, text size 1
    const int charW = 6;
    const int charH = 8;

    int maxChars = 0;
    for (int i = 0; i < lineCount; ++i) {
        int len = (int)strlen(lines[i]);
        if (len > maxChars) maxChars = len;
    }

    int totalH = lineCount * charH;
    int startY = (MATRIX_SCREEN_H - totalH) / 2;
    if (startY < 0) startY = 0;

    // Left edge so the widest line is centered; all lines share this x so
    // relative indentation inside the art is preserved.
    int xLeft = (320 - maxChars * charW) / 2;
    if (xLeft < 0) xLeft = 0;

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);

    for (int i = 0; i < lineCount; ++i) {
        int y = startY + i * charH;
        M5.Lcd.setCursor(xLeft, y);
        M5.Lcd.print(lines[i]);
    }
}

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
                } else if (key == "user") {
                    next.user = value;
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

    // Section labels (all text size 2), spaced evenly: TIME, IP, NET, CPU, RAM
    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Lcd.setCursor(4, 38);
    M5.Lcd.print("TIME");
    M5.Lcd.setCursor(4, 80);
    M5.Lcd.print("IP");
    M5.Lcd.setCursor(4, 122);
    M5.Lcd.print("NET");
    M5.Lcd.setCursor(4, 164);
    M5.Lcd.print("CPU");
    M5.Lcd.setCursor(4, 206);
    M5.Lcd.print("RAM");
}

static void drawHeaderAndIpIfNeeded() {
    // Only redraw header/IP when they actually change (or first time)
    if (!g_headerIpInitialized ||
        g_prevUser != g_stats.user ||
        g_prevHostname != g_stats.hostname ||
        g_prevLocalIp != g_stats.localIp ||
        g_prevPublicIp != g_stats.publicIp) {

        g_headerIpInitialized = true;
        g_prevUser           = g_stats.user;
        g_prevHostname       = g_stats.hostname;
        g_prevLocalIp        = g_stats.localIp;
        g_prevPublicIp       = g_stats.publicIp;

        // Header: user@hostname (no //STATUS)
        M5.Lcd.fillRect(3, 3, 314, 22, TFT_BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Lcd.setCursor(4, 4);
        if (g_stats.user.length() > 0 && g_stats.hostname.length() > 0) {
            M5.Lcd.printf("%s@%s", g_stats.user.c_str(), g_stats.hostname.c_str());
        } else if (g_stats.hostname.length() > 0) {
            M5.Lcd.print(g_stats.hostname.c_str());
        } else {
            M5.Lcd.print("?@?");
        }

        // IP row (two lines)
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.fillRect(72, 78, 244, 28, TFT_BLACK);
        M5.Lcd.setCursor(72, 82);
        if (g_stats.localIp.length() > 0) {
            M5.Lcd.printf("LAN %s", g_stats.localIp.c_str());
        } else {
            M5.Lcd.print("LAN n/a");
        }
        M5.Lcd.setCursor(72, 98);
        if (g_stats.publicIp.length() > 0) {
            M5.Lcd.printf("WAN %s", g_stats.publicIp.c_str());
        } else {
            M5.Lcd.print("WAN n/a");
        }
    }
}

static void drawDynamicStats() {
    // Header with hostname
    // Common text settings for dynamic stats
    M5.Lcd.setTextSize(2);

    // TIME row
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.fillRect(72, 36, 244, 20, TFT_BLACK);
    M5.Lcd.setCursor(72, 40);
    if (g_stats.time.length() > 0) {
        M5.Lcd.print(g_stats.time);
    } else {
        M5.Lcd.print("waiting...");
    }

    // NET row (up/down speeds below IP)
    M5.Lcd.fillRect(72, 124, 244, 16, TFT_BLACK);
    M5.Lcd.setCursor(72, 124);
    M5.Lcd.printf("UP:%.2fMB DW:%.2fMB", g_stats.netUpMbps, g_stats.netDownMbps);

    // CPU bar
    int barX    = 72;
    int barY    = 166;
    int gap     = 6;
    int cpuBarW = 145;
    int barH    = 18;
    drawBar(barX, barY, cpuBarW, barH, g_stats.cpu, TFT_PURPLE);

    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(barX + cpuBarW + gap, barY);
    if (g_stats.cpuTempC > 0.0f) {
        M5.Lcd.printf("%dC ", (int)(g_stats.cpuTempC + 0.5f));
    } else {
        M5.Lcd.print("- ");
    }
    int cpuPct = (int)(g_stats.cpu + 0.5f);
    if (cpuPct < 10) M5.Lcd.print(" ");
    M5.Lcd.print(cpuPct);
    M5.Lcd.print("%");

    // RAM bar
    int ramY    = 208;
    int ramBarW = 190;
    drawBar(barX, ramY, ramBarW, barH, g_stats.ramPercent, TFT_RED);

    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(barX + ramBarW + gap, ramY);
    if (g_stats.ramTotalMb > 0) {
        int ramPct = (int)(g_stats.ramPercent + 0.5f);
        if (ramPct < 10) M5.Lcd.print(" ");
        M5.Lcd.print(ramPct);
    } else {
        M5.Lcd.print("-");
    }
    M5.Lcd.print("%");
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

// ============================================================================
// Matrix rain rendering
// ============================================================================

static void initMatrixMode() {
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextSize(1);
    M5.Lcd.fillScreen(TFT_BLACK);

    // Initialize random drop positions and trail lengths
    int rows = MATRIX_SCREEN_H / MATRIX_TEXT_HEIGHT;
    for (int i = 0; i < MATRIX_COLS; ++i) {
        g_matrixDropY[i] = - (random(rows));  // start at random negative row
        g_matrixTrailLen[i] =
            random(MATRIX_TRAIL_MIN_ROWS, MATRIX_TRAIL_MAX_ROWS + 1);
    }
    g_matrixInitialized = true;
}

static void matrixStep() {
    if (!g_matrixInitialized) {
        initMatrixMode();
    }

    int rows = MATRIX_SCREEN_H / MATRIX_TEXT_HEIGHT;

    // For each column, advance a single "raindrop"
    for (int col = 0; col < MATRIX_COLS; ++col) {
        int headRow = g_matrixDropY[col];
        int headY   = headRow * MATRIX_TEXT_HEIGHT;
        int x       = col * 8;  // 8px spacing across 320px

        // Erase the tail segment that has moved beyond our desired (per-column) trail length
        int tailRow = headRow - g_matrixTrailLen[col];
        if (tailRow >= 0 && tailRow * MATRIX_TEXT_HEIGHT < MATRIX_SCREEN_H) {
            int tailY = tailRow * MATRIX_TEXT_HEIGHT;
            // Clear the entire character cell so old trail pixels fully disappear
            M5.Lcd.fillRect(x, tailY, 8, MATRIX_TEXT_HEIGHT, TFT_BLACK);
        }

        // Draw mid-trail in dark green (one row behind head)
        int midRow = headRow - 1;
        if (midRow >= 0 && midRow * MATRIX_TEXT_HEIGHT < MATRIX_SCREEN_H) {
            int midY = midRow * MATRIX_TEXT_HEIGHT;
            M5.Lcd.setTextColor(TFT_DARKGREEN, TFT_BLACK);
            M5.Lcd.drawChar((char)random(32, 128), x, midY, 1);
        }

        // Leading bright character
        if (headY >= 0 && headY < MATRIX_SCREEN_H) {
            M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Lcd.drawChar((char)random(32, 128), x, headY, 1);
        }

        g_matrixDropY[col] += 1;
        if (g_matrixDropY[col] >= rows + g_matrixTrailLen[col]) {
            // Once both head and trail are off-screen, restart above with a new random trail length
            g_matrixDropY[col]    = -random(rows);
            g_matrixTrailLen[col] =
                random(MATRIX_TRAIL_MIN_ROWS, MATRIX_TRAIL_MAX_ROWS + 1);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    M5.begin(true, false, true, false);
    M5.Power.setPowerWLEDSet(false);

    M5.Lcd.setRotation(1);  // wide layout for dashboard
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextSize(2);

    g_mode               = MODE_DASHBOARD;
    g_matrixInitialized  = false;
    g_currentArtIndex    = 0;
    g_headerIpInitialized = false;
    drawStaticFrame();

    Serial.println("=== M5Core1 Cyberdeck Status Display ===");
    Serial.println("Waiting for serial stats lines from host...");
}

void loop() {
    M5.update();
    processSerialInput();

    // Mode switching: BtnA = dashboard, BtnB = ASCII art, BtnC = matrix
    if (M5.BtnA.wasPressed()) {
        g_mode = MODE_DASHBOARD;
        M5.Lcd.setRotation(1);
        M5.Lcd.setTextFont(1);
        M5.Lcd.setTextSize(2);
        M5.Lcd.fillScreen(TFT_BLACK);
        g_headerIpInitialized = false;
        drawStaticFrame();
    }
    if (M5.BtnB.wasPressed()) {
        if (g_mode != MODE_ART) {
            g_mode            = MODE_ART;
            g_currentArtIndex = 0;
        } else {
            if (g_numAsciiArts > 0) {
                g_currentArtIndex = (g_currentArtIndex + 1) % g_numAsciiArts;
            }
        }
        renderCurrentArt();
    }
    if (M5.BtnC.wasPressed()) {
        g_mode              = MODE_MATRIX;
        g_matrixInitialized = false;  // re-init next frame
    }

    if (g_mode == MODE_DASHBOARD) {
        uint32_t now = millis();
        if (now - g_lastRedrawMs > 200) {
            g_lastRedrawMs = now;
            g_animPhase++;
            drawHeaderAndIpIfNeeded();
            drawDynamicStats();
        }
    } else if (g_mode == MODE_MATRIX) {
        matrixStep();
        delay(30);
    } else if (g_mode == MODE_ART) {
        // Nothing to do per-frame; ASCII art is static until BtnB is pressed again
    }

    delay(5);
}

