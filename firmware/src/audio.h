#pragma once
#include "config.h"

struct PadSlot {
    char      name[10] = {0};
    uint8_t   kind     = 0;   // 0 empty, 1 synth, 2 sample, 3 missing
    int16_t*  data     = nullptr;
    uint32_t  frames   = 0;
    float     step     = 1.0f;
    uint8_t   synth    = 0;
};

class AudioEngine {
public:
    bool begin();
    void start();

    void trigger(uint8_t pad, uint8_t vel);
    void release(uint8_t pad);

    void setPlaying(bool on);
    void setBpm(uint8_t bpm);
    void setSwing(uint8_t swing);
    void setVolume(uint8_t v);

    bool     playing() const { return _playing; }
    uint8_t  playhead() const { return _playhead; }
    uint8_t  level(uint8_t pad) const { return _level[pad]; }
    uint16_t underruns() const { return _underruns; }
    uint8_t  cpuPct() const { return _cpu; }
    uint8_t  bufPct() const { return _bufPct; }
    bool     codecOk() const { return _codecOk; }

    int16_t* alloc(uint32_t frames);
    int16_t* allocRecord(uint32_t frames);
    void     freeLast(int16_t* p, uint32_t frames);
    void     resetPool() { _poolUsed = 0; }
    uint32_t poolUsed() const { return _poolUsed; }
    uint32_t poolFree() const { return POOL - _poolUsed; }
    static constexpr uint32_t POOL = POOL_SAMPLES;

    // mic sampling (runs on the audio task; I2S is switched TX -> RX -> TX)
    enum : uint8_t { REC_IDLE = 0, REC_RUNNING = 1, REC_DONE = 2 };
    bool     startRecord(int16_t* buf, uint32_t maxFrames);
    void     cancelRecord();
    uint8_t  recordState() const { return _recState; }
    uint32_t recordPos() const { return _recPos; }

    PadSlot  pads[16];
    Step     pattern[8][16];
    uint8_t  trackPad[8]  = {0, 1, 2, 3, 4, 5, 6, 7};
    bool     trackMute[8] = {false};
    bool     trackSolo[8] = {false};

private:
    struct Voice {
        bool            active = false;
        uint8_t         pad    = 0;
        uint8_t         kind   = 0;   // 1 synth, 2 sample
        uint8_t         synth  = 0;
        const int16_t*  data   = nullptr;
        uint32_t        frames = 0;
        float           pos    = 0;
        float           step   = 1.0f;
        float           gain   = 0;
        float           env    = 0;
        float           envMul = 0.9999f;
        float           phase  = 0;
        float           inc    = 0;
        float           incMin = 0;
        float           incMul = 0.9999f;
        float           noise  = 0;
        float           hp     = 0;
        float           lp     = 0;
        uint32_t        age    = 0;
    };

    struct Event {
        uint8_t pad;
        uint8_t vel;
        uint8_t on;
    };

    void taskLoop();
    static void taskEntry(void* arg);
    void doRecord();
    void drainEvents();
    void sequencer();
    void render(int16_t* out, int frames);
    void startVoice(uint8_t pad, uint8_t vel);
    void initSynthVoice(Voice& v, uint8_t synth, float vel);
    float sine(float phase) const;

    Voice     _v[MAX_VOICES];
    int16_t   _pool[POOL_SAMPLES];
    uint32_t  _poolUsed = 0;
    uint32_t  _reserve = 24000;  // keep ~1.1 s at 22.05 kHz free for sampling
    int16_t*  _recBuf = nullptr;
    uint32_t  _recCap = 0;
    volatile uint32_t _recPos = 0;
    volatile uint8_t  _recState = REC_IDLE;
    volatile uint8_t  _recStart = 0;
    volatile uint8_t  _recCancel = 0;
    float     _sine[1024];
    uint8_t   _level[16] = {0};
    uint16_t  _underruns = 0;
    uint8_t   _cpu = 0;
    uint8_t   _bufPct = 100;
    bool      _codecOk = false;
    bool      _playing = false;
    uint8_t   _playhead = 0;
    uint8_t   _bpm = 92;
    uint8_t   _swing = 54;
    uint8_t   _volume = 78;
    float     _samplesPerTick = 300.0f;
    float     _tickAcc = 0;
    uint32_t  _tick = 0;

    volatile uint8_t _qHead = 0, _qTail = 0;
    Event    _q[32];

    void*    _taskHandle = nullptr;
};

extern AudioEngine audio;
