#pragma once
#include <stddef.h>
#include "config.h"

struct PadSlot;

class SampleStore {
public:
    bool begin();
    int  scanKits();
    bool loadKit(int index, void (*progress)(int pct, const char* name));
    void loadSynthKit();
    int  saveRecording(const PadSlot& pad, char* nameOut, size_t nameLen);

    bool     ready() const { return _ready; }
    uint32_t totalMB() const { return _totalMB; }
    uint32_t freeMB() const { return _freeMB; }
    float    readMBs() const { return _readMBs; }
    const char* cardName() const { return _cardName; }

    float testRead();

private:
    bool loadWav(const char* path, uint8_t pad, uint32_t* bytesOut);
    bool _ready = false;
    uint32_t _totalMB = 0;
    uint32_t _freeMB  = 0;
    float    _readMBs = 0;
    char     _cardName[16] = "NONE";
};

extern SampleStore store;
