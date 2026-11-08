#pragma once
#include <stdint.h>

// ---------------------------------------------------------------- hardware
#define PIN_I2C_SDA   8
#define PIN_I2C_SCL   9
#define PIN_I2S_BCLK  41
#define PIN_I2S_LRCK  43
#define PIN_I2S_DOUT  42
#define PIN_I2S_DIN   46

#define PIN_SD_SCK    40
#define PIN_SD_MISO   39
#define PIN_SD_MOSI   14
#define PIN_SD_CS     12

#define PIN_TCA_INT   11

// ---------------------------------------------------------------- audio
static constexpr uint32_t AUDIO_RATE   = 44100;
static constexpr int      AUDIO_BLOCK  = 128;
static constexpr int      MAX_VOICES   = 16;
static constexpr int      POOL_SAMPLES = 65536;  // 128 KB of sample RAM

// ---------------------------------------------------------------- layout
static constexpr int SCR_W = 240;
static constexpr int SCR_H = 135;
static constexpr int TOP_H = 12;
static constexpr int BOT_H = 11;
static constexpr int BOT_Y = SCR_H - BOT_H;

// ---------------------------------------------------------------- Gruvbox Dark
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define C_BG      RGB565(29, 32, 33)
#define C_BAR     RGB565(40, 40, 40)
#define C_PANEL   RGB565(40, 40, 40)
#define C_PAD     RGB565(50, 48, 47)
#define C_EDGE    RGB565(80, 73, 69)
#define C_LINE    RGB565(60, 56, 54)
#define C_TXT     RGB565(235, 219, 178)
#define C_DIM     RGB565(168, 153, 132)
#define C_FAINT   RGB565(124, 111, 100)
#define C_ACC     RGB565(254, 128, 25)
#define C_ACC_DIM RGB565(74, 42, 23)
#define C_AMB     RGB565(250, 189, 47)
#define C_AQUA    RGB565(142, 192, 124)
#define C_BLUE    RGB565(131, 165, 152)
#define C_RED     RGB565(251, 73, 52)
#define C_GREEN   RGB565(184, 187, 38)

// ---------------------------------------------------------------- misc
enum class Page : uint8_t { PAD, SEQ, SMP, MIX, SYS, SD, KIT, LOADING, NOSD };
enum class SdState : uint8_t { NONE, MOUNTING, READY, DEGRADED, ERROR };

struct Step {
    uint8_t on  = 0;
    uint8_t vel = 100;
};

struct KitInfo {
    char     name[20] = {0};
    uint16_t samples  = 0;
    uint32_t bytes    = 0;
};
