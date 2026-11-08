#include "ui.h"

#include <Arduino.h>
#include <stdarg.h>

#include "audio.h"
#include "font8.h"
#include "samples.h"

UI ui;

static const char* TAB_NAMES[6] = {"PAD", "SEQ", "SMP", "MIX", "SYS", "KIT"};
static const char* TRACK_LABELS[8] = {"KIK", "SNR", "CLP", "HAT", "OPN", "TOM", "RIM", "FX"};

// ---------------------------------------------------------------- primitives
void UI::begin()
{
    _c.setColorDepth(16);
    _c.createSprite(SCR_W, SCR_H);
    _c.fillSprite(C_BG);
    _c.setTextColor(C_TXT);
}

void UI::clear(uint16_t col) { _c.fillSprite(col); }
void UI::px(int x, int y, uint16_t col)
{
    if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H) _c.drawPixel(x, y, col);
}

void UI::rect(int x, int y, int w, int h, uint16_t col)
{
    _c.fillRect(x, y, w, h, col);
}

void UI::frame(int x, int y, int w, int h, uint16_t col)
{
    _c.drawRect(x, y, w, h, col);
}

void UI::hline(int x, int y, int w, uint16_t col)
{
    _c.drawFastHLine(x, y, w, col);
}

int UI::tw(const char* s) const { return (int)strlen(s) * FONT_W; }

void UI::text(int x, int y, const char* s, uint16_t col, bool bold)
{
    const uint8_t (*tbl)[FONT_H] = bold ? FONT8B : FONT8;
    int cx = x;
    for (const char* p = s; *p; ++p) {
        char c = *p;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 95) c = '?';
        const uint8_t* g = tbl[c - 32];
        for (int row = 0; row < FONT_H; ++row) {
            uint8_t bits = g[row];
            for (int b = 0; b < FONT_W; ++b) {
                if (bits & (1 << b)) px(cx + b, y + row, col);
            }
        }
        cx += FONT_W;
        if (cx > SCR_W) break;
    }
}

void UI::textf(int x, int y, uint16_t col, bool bold, const char* fmt, ...)
{
    char buf[48];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    text(x, y, buf, col, bold);
}

void UI::bar(int x, int y, int w, int h, float ratio, uint16_t fg)
{
    rect(x, y, w, h, C_EDGE);
    int fw = (int)((w - 2) * (ratio < 0 ? 0 : (ratio > 1 ? 1 : ratio)));
    if (fw > 0) rect(x + 1, y + 1, fw, h - 2, fg);
}

void UI::chip(int x, int y, int w, int h, const char* s, bool active, bool danger)
{
    if (active) {
        rect(x, y, w, h, C_ACC);
        text(x + (w - tw(s)) / 2, y + 3, s, C_BG, true);
    } else {
        frame(x, y, w, h, danger ? C_RED : C_EDGE);
        text(x + (w - tw(s)) / 2, y + 3, s, danger ? C_RED : C_DIM);
    }
}

void UI::battery(int x, int y, int pct)
{
    frame(x, y, 12, 6, C_DIM);
    rect(x + 12, y + 2, 2, 2, C_DIM);
    int n = pct > 70 ? 8 : (pct > 35 ? 5 : 2);
    rect(x + 1, y + 1, n, 4, pct > 35 ? C_GREEN : C_RED);
}

void UI::sdtag(int x, int y)
{
    uint16_t col = app.sd == SdState::READY ? C_GREEN
                 : app.sd == SdState::MOUNTING ? C_AMB
                 : C_RED;
    text(x, y, "SD", col, false);
}

int UI::tabIndex() const
{
    switch (app.page) {
        case Page::PAD: return 0;
        case Page::SEQ: return 1;
        case Page::SMP: return 2;
        case Page::MIX: return 3;
        case Page::SYS: return 4;
        case Page::KIT: return 5;
        case Page::SD:  return 4;
        default:        return 0;
    }
}

void UI::topbar()
{
    rect(0, 0, SCR_W, TOP_H, C_BAR);
    hline(0, TOP_H - 1, SCR_W, C_LINE);

    int active = tabIndex();
    for (int i = 0; i < 6; ++i) {
        int x = 3 + i * 24;
        if (i == active) {
            text(x, 2, TAB_NAMES[i], C_ACC, true);
            hline(x, 10, tw(TAB_NAMES[i]), C_ACC);
        } else {
            text(x, 2, TAB_NAMES[i], C_FAINT);
        }
    }

    textf(150, 2, C_TXT, true, "%d.0", app.bpm);
    text(178, 2, "BPM", C_FAINT);
    sdtag(204, 2);
    battery(224, 3, app.battery);
}

void UI::bottombar(const char* s)
{
    rect(0, BOT_Y, SCR_W, BOT_H, C_BAR);
    hline(0, BOT_Y, SCR_W, C_LINE);
    text(3, BOT_Y + 2, s, C_FAINT);
}

void UI::toast()
{
    if (!app.status[0] || millis() > app.statusUntil) return;
    int w = tw(app.status) + 8;
    if (w > SCR_W - 8) w = SCR_W - 8;
    rect(4, BOT_Y - 16, w, 13, C_PANEL);
    frame(4, BOT_Y - 16, w, 13, C_ACC);
    text(8, BOT_Y - 13, app.status, C_TXT);
}

// ---------------------------------------------------------------- PAD
void UI::drawSynthWave(int x, int y, int w, int h, uint8_t synth, uint16_t col)
{
    float cy = y + h / 2.0f;
    for (int i = 0; i < w; ++i) {
        float t = (float)i / w;
        float env = expf(-t * (synth == 0 ? 5.0f : 8.0f));
        float s = sinf(t * (synth == 0 ? 40.0f : 90.0f)) * env;
        int hh = (int)(s * (h / 2 - 2));
        if (hh) px(x + i, (int)cy - hh, col), px(x + i, (int)cy + hh, col);
    }
}

void UI::drawWave(int x, int y, int w, int h, int pad, uint16_t col)
{
    PadSlot& p = audio.pads[pad];
    if (p.kind != 2 || !p.data || p.frames < 2) {
        drawSynthWave(x, y, w, h, p.synth, col);
        return;
    }
    int cy = y + h / 2;
    for (int i = 0; i < w; ++i) {
        uint32_t a = (uint32_t)((uint64_t)i * p.frames / w);
        uint32_t b = (uint32_t)((uint64_t)(i + 1) * p.frames / w);
        if (b <= a) b = a + 1;
        int16_t mn = 32767, mx = -32768;
        uint32_t step = (b - a) / 64 + 1;
        for (uint32_t k = a; k < b; k += step) {
            int16_t v = p.data[k];
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        int y0 = cy - (int)((float)mx / 32768.0f * (h / 2 - 2));
        int y1 = cy - (int)((float)mn / 32768.0f * (h / 2 - 2));
        if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
        for (int yy = y0; yy <= y1; ++yy) px(x + i, yy, col);
    }
}

void UI::pageLevels()
{
    for (int i = 0; i < 16; ++i) {
        int x = 4 + (i % 4) * 30;
        int y = 15 + (i / 4) * 27;
        float k = i / 15.0f;
        uint8_t vel = (uint8_t)(8 + i * 119 / 15);
        rect(x, y, 28, 25, RGB565(46 + (int)(40 * k), 34 + (int)(16 * k), 24 + (int)(8 * k)));
        frame(x, y, 28, 25, i == app.selected ? C_ACC : C_EDGE);
        textf(x + 2, y + 2, C_FAINT, false, "%d", i + 1);
        textf(x + 5, y + 10, k > 0.35f ? RGB565(251, 241, 199) : C_DIM, true, "%d", vel);
    }
    frame(4 + (app.selected % 4) * 30 - 1, 15 + (app.selected / 4) * 27 - 1, 30, 27, RGB565(189, 174, 147));

    int px0 = 127;
    PadSlot& p = audio.pads[app.selected];
    text(px0, 15, p.kind ? p.name : "EMPTY", C_TXT, true);
    text(px0, 26, "16 LEVELS", C_ACC);
    hline(px0, 36, 109, C_LINE);
    text(px0, 44, "VELOCITY", C_DIM);
    textf(px0, 54, C_ACC, true, "%d", app.levelValue);
    for (int i = 0; i < 16; ++i) {
        rect(px0 + i * 6, 66, 5, 7, i < (app.levelValue * 16 / 127) ? C_ACC : C_EDGE);
    }
    text(px0, 82, "RANGE 8-127", C_FAINT);
    text(px0, 94, "OPT HELD", C_ACC);
    textf(px0, 106, C_FAINT, false, "PAD %02d", app.selected + 1);

    bottombar("TAP PAD SET VEL  RELEASE OPT EXIT");
}

void UI::pageRec()
{
    int x = 18, y = 26, w = 204, h = 80;
    rect(x, y, w, h, C_PANEL);
    frame(x, y, w, h, C_RED);

    text(x + 8, y + 8, "SAMPLING", C_RED, true);
    textf(x + 132, y + 8, C_DIM, false, "PAD %02d", app.selected + 1);

    float pct = app.recMax ? (float)app.recPos / app.recMax : 0.0f;
    bar(x + 8, y + 24, w - 16, 8, pct, C_RED);
    textf(x + 8, y + 38, C_TXT, true, "%.1fS / %.1fS",
          app.recPos / 22050.0f, app.recMax / 22050.0f);
    textf(x + 100, y + 38, C_FAINT, false, "%d%%", (int)(pct * 100));

    text(x + 8, y + 54, "MIC -> PAD  AUTO SAVE SD", C_FAINT);
    text(x + 8, y + 66, "BKS CANCEL", C_DIM);

    bottombar("BKS CANCEL");
}

void UI::pagePad()
{
    if (app.levelsMode) { pageLevels(); return; }

    for (int i = 0; i < 16; ++i) {
        int x = 4 + (i % 4) * 30;
        int y = 15 + (i / 4) * 27;
        PadSlot& p = audio.pads[i];
        bool playing = audio.level(i) > 6;
        bool sel = (i == app.selected);

        if (playing) {
            rect(x, y, 28, 25, RGB565(63, 42, 27));
            frame(x, y, 28, 25, C_ACC);
        } else {
            rect(x, y, 28, 25, C_PAD);
            frame(x, y, 28, 25, C_EDGE);
        }
        if (sel) frame(x - 1, y - 1, 30, 27, RGB565(189, 174, 147));

        if (p.kind == 0) {
            text(x + 2, y + 3, "--", C_FAINT);
        } else {
            char sn[5] = {0};
            strncpy(sn, p.name, 4);
            text(x + 2, y + 3, sn, playing ? RGB565(251, 241, 199) : C_TXT, playing);
        }
        textf(x + 2, y + 14, playing ? C_ACC : C_FAINT, false, "%d", i + 1);

        if (p.kind == 2) text(x + 21, y + 13, "R", C_DIM, true);
        else if (p.kind == 3) text(x + 21, y + 13, "!", C_RED, true);

        uint8_t lv = audio.level(i);
        if (lv > 4) {
            rect(x + 3, y + 20, 22, 2, C_ACC_DIM);
            rect(x + 3, y + 20, 22 * lv / 127, 2, C_ACC);
        }
    }

    // right panel
    int px0 = 127;
    PadSlot& p = audio.pads[app.selected];
    text(px0, 15, p.kind ? p.name : "EMPTY", C_TXT, true);
    if (p.kind == 2) textf(px0, 26, C_FAINT, false, "%.2fS %dK R", (float)p.frames / AUDIO_RATE, (int)(p.step * 44));
    else if (p.kind == 1) text(px0, 26, "SYNTH", C_FAINT);
    else text(px0, 26, "NO SAMPLE", C_FAINT);
    hline(px0, 36, 109, C_LINE);

    rect(px0, 39, 109, 16, RGB565(24, 26, 27));
    frame(px0, 39, 109, 16, C_LINE);
    drawWave(px0 + 1, 41, 107, 12, app.selected, RGB565(205, 108, 60));

    static const char* names[4] = {"VOL", "TUNE", "CUT", "DEC"};
    static const float vals[4] = {0.82f, 0.50f, 0.74f, 0.38f};
    for (int i = 0; i < 4; ++i) {
        int y = 60 + i * 14;
        text(px0, y, names[i], C_DIM);
        bar(px0 + 32, y + 2, 76, 5, vals[i], i ? C_BLUE : C_ACC);
    }

    chip(px0, 112, 40, 11, "1-SHOT", true);
    chip(px0 + 44, 112, 34, 11, "CHOKE");
    chip(px0 + 82, 112, 24, 11, "16LV");

    bottombar("FN+1-4 BANK  OPT 16LV  SPC PLAY");
}

// ---------------------------------------------------------------- SEQ
void UI::pageSeq()
{
    text(3, 14, "PAT 01", C_ACC, true);
    text(52, 14, "BAR 1/4", C_FAINT);
    textf(118, 14, C_FAINT, false, "SW %d", app.swing);
    text(214, 14, "REC", C_RED);
    rect(232, 16, 4, 4, C_RED);

    const int x0 = 25, y0 = 24;
    int ph = audio.playhead();

    for (int tr = 0; tr < 8; ++tr) {
        int y = y0 + tr * 12;
        text(3, y + 1, TRACK_LABELS[tr], C_DIM);

        for (int c = 0; c < 16; ++c) {
            int x = x0 + c * 13;
            bool beat = (c % 4) == 0;
            uint16_t base = (app.playing && c == ph) ? C_EDGE : (beat ? RGB565(44, 41, 40) : RGB565(36, 34, 33));
            rect(x, y, 12, 9, base);
            Step& s = audio.pattern[tr][c];
            if (s.on) {
                float k = s.vel / 127.0f;
                rect(x + 1, y + 1, 10, 7, RGB565(120 + (int)(134 * k), 58 + (int)(70 * k), 30 - (int)(5 * k)));
            }
        }
    }

    if (app.playing) {
        int x = x0 + ph * 13;
        _c.fillTriangle(x + 2, 21, x + 9, 21, x + 5, 23, C_TXT);
    }
    int cx = x0 + app.cursorStep * 13;
    int cy = y0 + app.cursorTrack * 12;
    frame(cx, cy, 12, 9, C_TXT);

    bottombar("FN ARROWS MOVE  ENT TOGGLE  SPC PLAY");
}

// ---------------------------------------------------------------- SMP
void UI::pageSmp()
{
    PadSlot& p = audio.pads[app.selected];
    text(3, 14, p.kind ? p.name : "EMPTY", C_TXT, true);
    text(58, 14, p.kind == 2 ? "RAM" : "SYNTH", C_DIM);
    textf(196, 14, C_DIM, false, "%.2fS", p.kind == 2 ? (float)p.frames / AUDIO_RATE : 0.42f);

    int x = 3, y = 24, w = 233, h = 58;
    rect(x, y, w, h, RGB565(24, 26, 27));
    frame(x, y, w, h, C_LINE);
    hline(x, y + h / 2, w, RGB565(60, 56, 54));
    drawWave(x + 1, y + 2, w - 2, h - 4, app.selected, RGB565(215, 118, 76));

    int sx = 10, ex = 212;
    _c.drawFastVLine(x + sx, y, h, C_AQUA);
    _c.drawFastVLine(x + ex, y, h, C_AQUA);
    _c.fillTriangle(x + sx - 3, y, x + sx + 3, y, x + sx, y + 4, C_AQUA);
    _c.fillTriangle(x + ex - 3, y, x + ex + 3, y, x + ex, y + 4, C_AQUA);

    for (int k = 0; k < 4; ++k) {
        int tx = x + 1 + (int)(k * 0.1f / 0.42f * (w - 2));
        _c.drawFastVLine(tx, y + h - 4, 4, RGB565(96, 89, 82));
        textf(tx + 2, y + h - 9, C_FAINT, false, "0.%d", k);
    }

    text(3, 86, "ST 0.000", C_DIM);
    text(62, 86, "END 0.420", C_DIM);
    text(126, 86, "LEN 0.420", C_DIM);
    text(196, 86, "22.05K", C_FAINT);

    chip(3, 100, 32, 12, "NORM");
    chip(39, 100, 26, 12, "REV");
    chip(69, 100, 32, 12, "TRIM");
    chip(105, 100, 50, 12, "SLICE 8", true);
    chip(203, 100, 32, 12, "ZOOM");

    bottombar("FN+,- TRIM  ENT PREVIEW  BKS BACK");
}

// ---------------------------------------------------------------- MIX
void UI::pageMix()
{
    text(3, 14, "TRK", C_FAINT);
    text(52, 14, "LEVEL", C_FAINT);
    text(168, 14, "FX SEND", C_FAINT);

    static const float base[8] = {0.82f, 0.74f, 0.48f, 0.61f, 0.44f, 0.52f, 0.28f, 0.36f};
    for (int i = 0; i < 8; ++i) {
        int y = 23 + i * 12;
        bool mute = audio.trackMute[i];
        text(3, y + 1, TRACK_LABELS[i], mute ? C_FAINT : C_TXT);

        frame(25, y, 9, 9, C_EDGE);
        if (mute) rect(26, y + 1, 7, 7, C_ACC_DIM);
        text(27, y + 1, "M", mute ? C_ACC : C_FAINT);

        frame(37, y, 9, 9, C_EDGE);
        text(39, y + 1, "S", C_FAINT);

        float lv = app.playing ? audio.level(audio.trackPad[i]) / 127.0f : base[i];
        bar(50, y + 2, 104, 6, lv, mute ? C_ACC_DIM : C_ACC);
        bar(168, y + 2, 64, 6, base[7 - i] * 0.6f, C_AQUA);
    }

    bottombar("M/S MUTE SOLO  FN ARROWS  ENT SOLO");
}

// ---------------------------------------------------------------- SYS
void UI::pageSys()
{
    textf(4, 16, C_GREEN, true, "BATT %d%%", app.battery);
    bar(4, 26, 100, 4, app.battery / 100.0f, C_GREEN);

    text(4, 36, "CPU", C_DIM);
    textf(34, 36, C_TXT, true, "%d%%", audio.cpuPct());
    bar(4, 45, 110, 4, audio.cpuPct() / 100.0f, C_BLUE);

    text(4, 52, "MEM", C_DIM);
    textf(34, 52, C_TXT, true, "%dK", ESP.getFreeHeap() / 1024);
    bar(4, 61, 110, 4, 1.0f - ESP.getFreeHeap() / 320000.0f, C_AMB);

    text(4, 70, "VOICES 16", C_DIM);
    text(4, 82, "PADS 16/16", C_DIM);

    if (app.sd == SdState::READY) {
        text(124, 16, "SD READY", C_GREEN, true);
        textf(124, 28, C_TXT, true, "%.1fG FREE", app.sdFreeMB / 1024.0f);
        textf(124, 38, C_FAINT, false, "%.1f MB/S", app.sdReadMBs);
        textf(124, 50, C_DIM, false, "STALL %d", app.stalls);
    } else {
        text(124, 16, "SD NONE", C_RED, true);
    }
    text(124, 62, "MIDI BLE OFF", C_FAINT);
    text(124, 74, "FW 0.4.1", C_FAINT);

    rect(4, 96, 232, 24, RGB565(24, 26, 27));
    frame(4, 96, 232, 24, C_LINE);
    text(8, 105, "UNDERRUN", C_DIM);
    textf(62, 105, C_GREEN, true, "%d", app.underruns);
    textf(80, 105, C_FAINT, false, "LAT %.1fMS", 1000.0f * AUDIO_BLOCK / AUDIO_RATE);
    textf(160, 105, C_FAINT, false, "POOL %d%%", (int)(audio.poolUsed() * 100 / audio.POOL));

    bottombar("ENT SD  BKS BACK  HOLD G0 RESET");
}

// ---------------------------------------------------------------- SD
void UI::pageSd()
{
    text(4, 15, "SD CARD", C_TXT, true);
    text(204, 15, app.sd == SdState::READY ? "READY" : "ERROR", app.sd == SdState::READY ? C_GREEN : C_RED, true);

    float used = app.sdTotalMB ? 1.0f - (float)app.sdFreeMB / app.sdTotalMB : 0.0f;
    bar(4, 26, 232, 8, used, C_ACC);
    textf(4, 38, C_DIM, false, "USED %.1fG", (app.sdTotalMB - app.sdFreeMB) / 1024.0f);
    textf(180, 38, C_DIM, false, "FREE %.1fG", app.sdFreeMB / 1024.0f);

    textf(4, 50, C_TXT, false, "%s %dG  FAT32  CL10", store.cardName(), (int)(app.sdTotalMB / 1024));
    textf(4, 60, C_DIM, false, "READ %.1f MB/S  WRITE 0.9", app.sdReadMBs);
    textf(4, 70, C_DIM, false, "ERR 0   STALL %d   RETRY 0", app.stalls);
    textf(4, 80, C_AQUA, false, "BUF %d%%   STREAM 2/2", app.bufPct);
    textf(4, 90, C_FAINT, false, "%d KITS  -- PROJ  -- REC", app.kitCount);

    chip(4, 104, 34, 12, "TEST");
    chip(42, 104, 56, 12, "REMOUNT");
    chip(102, 104, 50, 12, "FORMAT", false, true);
    text(158, 107, "ERASES CARD", C_FAINT);

    bottombar("ENT TEST  FN+DEL FORMAT  BKS BACK");
}

// ---------------------------------------------------------------- KIT
void UI::pageKit()
{
    text(4, 15, "KITS", C_ACC, true);
    text(44, 15, "/KITS", C_FAINT);
    textf(176, 15, C_FAINT, false, "FREE %.1fG", app.sdFreeMB / 1024.0f);

    for (int i = 0; i < app.kitCount && i < 6; ++i) {
        int y = 27 + i * 15;
        bool sel = (i == app.kitSel);
        if (sel) {
            rect(2, y - 2, 236, 14, C_LINE);
            rect(2, y - 2, 2, 14, C_ACC);
        }
        text(8, y, app.kits[i].name, sel ? C_ACC : C_TXT, sel);
        textf(126, y, C_DIM, false, "%d/16", app.kits[i].samples);
        textf(166, y, C_FAINT, false, "%.1fM", app.kits[i].bytes / 1048576.0f);

        if (app.kits[i].samples == 0) text(200, y, "MISS", C_RED, true);
        else if (app.kits[i].bytes <= audio.POOL * 2) text(200, y, "RAM", C_DIM, true);
        else text(200, y, "STR", C_AQUA, true);
    }

    if (app.kitCount == 0) text(8, 40, "NO KITS ON CARD", C_FAINT);

    bottombar("ENT LOAD  BKS DEL  TAB BACK");
}

// ---------------------------------------------------------------- NO SD
void UI::pageNoSd()
{
    int cx = 100, cy = 30;
    frame(cx, cy, 40, 28, C_DIM);
    rect(cx + 4, cy + 4, 12, 4, C_DIM);
    _c.drawLine(cx + 2, cy + 2, cx + 37, cy + 25, C_RED);
    _c.drawLine(cx + 37, cy + 2, cx + 2, cy + 25, C_RED);

    const char* s1 = "NO SD CARD";
    text((SCR_W - tw(s1)) / 2, 66, s1, C_RED, true);
    const char* s2 = "INSERT A FAT32 CARD";
    text((SCR_W - tw(s2)) / 2, 78, s2, C_DIM);
    const char* s3 = "KITS AND SAMPLES LIVE ON SD";
    text((SCR_W - tw(s3)) / 2, 90, s3, C_FAINT);
    const char* s4 = app.sdError[0] ? app.sdError : "MOUNT FAILED  ERR 0X0D";
    text((SCR_W - tw(s4)) / 2, 104, s4, RGB565(96, 89, 82));

    bottombar("ENT RETRY   SYS DIAG");
}

// ---------------------------------------------------------------- loading
void UI::renderLoading(int pct, const char* name)
{
    _c.startWrite();
    clear(C_BG);
    rect(0, 0, SCR_W, TOP_H, C_BAR);
    hline(0, TOP_H - 1, SCR_W, C_LINE);
    text(3, 2, "LOADING", C_ACC, true);
    text(204, 2, "SD", C_AMB);

    text(12, 22, name, C_TXT, true);
    text(12, 32, "READING KIT FROM SD", C_FAINT);

    bar(12, 46, 216, 8, pct / 100.0f, C_ACC);
    textf(12, 60, C_ACC, true, "%d%%", pct);
    textf(44, 60, C_DIM, false, "%d / 16 PADS", (pct * 16) / 100);
    text(12, 74, "PRELOAD 6   STREAM 10", C_DIM);
    text(12, 90, "DO NOT REMOVE CARD", C_AMB);

    bottombar("BKS CANCEL");
    _c.endWrite();
    _c.pushSprite(0, 0);
}

// ---------------------------------------------------------------- render
void UI::render()
{
    _c.startWrite();
    clear(C_BG);

    if (app.page == Page::LOADING) {
        _c.endWrite();
        return;
    }

    topbar();

    if (app.recording) {
        pageRec();
        _c.endWrite();
        _c.pushSprite(0, 0);
        return;
    }

    switch (app.page) {
        case Page::PAD: pagePad(); break;
        case Page::SEQ: pageSeq(); break;
        case Page::SMP: pageSmp(); break;
        case Page::MIX: pageMix(); break;
        case Page::SYS: pageSys(); break;
        case Page::SD:  pageSd();  break;
        case Page::KIT: pageKit(); break;
        case Page::NOSD: pageNoSd(); break;
        default: break;
    }

    toast();
    _c.endWrite();
    _c.pushSprite(0, 0);
}
