#pragma once
#include "config.h"

struct AppState {
    Page     page    = Page::PAD;
    SdState  sd      = SdState::NONE;
    bool     playing = false;
    bool     recArm  = false;
    uint8_t  bank    = 0;
    uint8_t  selected = 0;
    uint8_t  cursorTrack = 0;
    uint8_t  cursorStep  = 0;
    uint8_t  bpm     = 92;
    uint8_t  swing   = 54;
    bool     levelsMode = false;
    uint8_t  levelValue = 64;
    uint8_t  volume  = 78;
    uint8_t  battery = 78;
    uint8_t  cpu     = 0;
    uint16_t underruns = 0;
    uint16_t stalls    = 0;
    uint8_t  bufPct    = 100;
    uint32_t sdTotalMB = 0;
    uint32_t sdFreeMB  = 0;
    float    sdReadMBs = 0;
    char     sdError[28] = {0};

    KitInfo  kits[8];
    int      kitCount = 0;
    int      kitSel   = 0;

    uint8_t  loadingPct = 0;
    char     loadingName[20] = {0};
    uint32_t sampleBytes = 0;
    bool     codecOk = false;

    char     status[40] = {0};
    uint32_t statusUntil = 0;
};

extern AppState app;
void setStatus(const char* s, uint32_t ms = 1600);
