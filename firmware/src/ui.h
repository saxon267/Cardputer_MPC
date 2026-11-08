#pragma once
#include <M5Cardputer.h>
#include "app.h"

class UI {
public:
    void begin();
    void render();
    void renderLoading(int pct, const char* name);

private:
    M5Canvas _c{&M5Cardputer.Display};

    void clear(uint16_t col);
    void px(int x, int y, uint16_t col);
    void rect(int x, int y, int w, int h, uint16_t col);
    void frame(int x, int y, int w, int h, uint16_t col);
    void hline(int x, int y, int w, uint16_t col);
    void text(int x, int y, const char* s, uint16_t col, bool bold = false);
    void textf(int x, int y, uint16_t col, bool bold, const char* fmt, ...);
    int  tw(const char* s) const;
    void bar(int x, int y, int w, int h, float ratio, uint16_t fg);
    void chip(int x, int y, int w, int h, const char* s, bool active = false, bool danger = false);
    void battery(int x, int y, int pct);
    void sdtag(int x, int y);
    void topbar();
    void bottombar(const char* s);
    void toast();
    int  tabIndex() const;

    void pagePad();
    void pageLevels();
    void pageSeq();
    void pageSmp();
    void pageMix();
    void pageSys();
    void pageSd();
    void pageKit();
    void pageNoSd();

    void drawWave(int x, int y, int w, int h, int pad, uint16_t col);
    void drawSynthWave(int x, int y, int w, int h, uint8_t synth, uint16_t col);
};

extern UI ui;
