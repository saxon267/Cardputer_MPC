#include "audio.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include <driver/i2s.h>
#include <math.h>

AudioEngine audio;

// ---------------------------------------------------------------- ES8311
static bool es8311Write(uint8_t reg, uint8_t val)
{
    return M5Cardputer.In_I2C.writeRegister8(0x18, reg, val, 400000);
}

static uint8_t dacReg(uint8_t volume)
{
    if (volume == 0) return 0x00;
    if (volume <= 50) return (uint8_t)(0x60 + (volume * (0xBF - 0x60)) / 50);
    return (uint8_t)(0xC0 + ((volume - 51) * (0xFF - 0xC0)) / 49);
}

static void es8311Init(uint8_t volume)
{
    es8311Write(0x00, 0x80);  // CSM on
    es8311Write(0x01, 0xB5);  // MCLK = BCLK, DAC clock on
    es8311Write(0x02, 0x18);  // MULT_PRE = 3
    es8311Write(0x09, 0x0C);  // 16-bit I2S in
    es8311Write(0x0A, 0x0C);  // 16-bit I2S out
    es8311Write(0x0D, 0x01);  // analog power
    es8311Write(0x12, 0x00);  // DAC on
    es8311Write(0x13, 0x10);  // HP driver
    es8311Write(0x32, dacReg(volume));
    es8311Write(0x37, 0x08);  // DAC EQ bypass
    delay(30);
}

static bool installI2s()
{
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = AUDIO_RATE;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 6;
    cfg.dma_buf_len          = AUDIO_BLOCK;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;
    cfg.fixed_mclk           = 0;
    cfg.mclk_multiple        = I2S_MCLK_MULTIPLE_256;
    cfg.bits_per_chan        = I2S_BITS_PER_CHAN_16BIT;

    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;

    i2s_pin_config_t pins = {};
    pins.mck_io_num   = I2S_PIN_NO_CHANGE;
    pins.bck_io_num   = PIN_I2S_BCLK;
    pins.ws_io_num    = PIN_I2S_LRCK;
    pins.data_out_num = PIN_I2S_DOUT;
    pins.data_in_num  = I2S_PIN_NO_CHANGE;
    return i2s_set_pin(I2S_NUM_0, &pins) == ESP_OK;
}

// ---------------------------------------------------------------- engine
bool AudioEngine::begin()
{
    for (int i = 0; i < 1024; ++i) _sine[i] = sinf((float)i * 6.2831853f / 1024.0f);

    _codecOk = M5Cardputer.In_I2C.scanID(0x18, 400000);
    if (_codecOk) es8311Init(_volume);

    if (!installI2s()) return false;
    for (int i = 0; i < 16; ++i) {
        pads[i].kind = 0;
    }
    return true;
}

void AudioEngine::start()
{
    xTaskCreatePinnedToCore(taskEntry, "audio", 4096, this, 20, (TaskHandle_t*)&_taskHandle, 1);
}

void AudioEngine::taskEntry(void* arg)
{
    static_cast<AudioEngine*>(arg)->taskLoop();
}

void AudioEngine::taskLoop()
{
    static int16_t out[AUDIO_BLOCK * 2];
    uint32_t lastUs = micros();

    while (true) {
        uint32_t t0 = micros();
        drainEvents();
        sequencer();
        render(out, AUDIO_BLOCK);

        size_t written = 0;
        esp_err_t err = i2s_write(I2S_NUM_0, out, sizeof(out), &written, portMAX_DELAY);
        if (err != ESP_OK) ++_underruns;

        uint32_t blockUs = (uint32_t)(1000000ULL * AUDIO_BLOCK / AUDIO_RATE);
        uint32_t spent = micros() - t0;
        uint32_t pct = spent * 100 / blockUs;
        _cpu = (uint8_t)(pct > 100 ? 100 : pct);

        uint32_t now = micros();
        if (now - lastUs > blockUs * 3 / 2) ++_underruns;
        lastUs = now;

        for (int i = 0; i < 16; ++i) _level[i] = (uint8_t)(_level[i] * 7 / 8);
    }
}

void AudioEngine::setVolume(uint8_t v)
{
    _volume = v > 100 ? 100 : v;
    if (_codecOk) es8311Write(0x32, dacReg(_volume));
}

void AudioEngine::setPlaying(bool on)
{
    if (on && !_playing) {
        _tick    = 0;
        _tickAcc = 0;
        _playhead = 0;
    }
    _playing = on;
}

void AudioEngine::setBpm(uint8_t bpm)
{
    _bpm = bpm < 40 ? 40 : (bpm > 240 ? 240 : bpm);
    _samplesPerTick = AUDIO_RATE * 60.0f / (_bpm * 96.0f);
}

void AudioEngine::setSwing(uint8_t s) { _swing = s; }

int16_t* AudioEngine::alloc(uint32_t frames)
{
    if (_poolUsed + frames > POOL_SAMPLES) return nullptr;
    int16_t* p = &_pool[_poolUsed];
    _poolUsed += frames;
    return p;
}

void AudioEngine::trigger(uint8_t pad, uint8_t vel)
{
    if (pad >= 16) return;
    uint8_t next = (uint8_t)((_qHead + 1) & 31);
    if (next == _qTail) return;
    _q[_qHead] = {pad, vel, 1};
    _qHead = next;
}

void AudioEngine::release(uint8_t pad)
{
    uint8_t next = (uint8_t)((_qHead + 1) & 31);
    if (next == _qTail) return;
    _q[_qHead] = {pad, 0, 0};
    _qHead = next;
}

void AudioEngine::drainEvents()
{
    while (_qTail != _qHead) {
        Event e = _q[_qTail];
        _qTail = (uint8_t)((_qTail + 1) & 31);
        if (e.on) {
            startVoice(e.pad, e.vel);
        } else {
            for (auto& v : _v) {
                if (v.active && v.pad == e.pad) v.envMul = 0.97f;
            }
        }
    }
}

float AudioEngine::sine(float phase) const
{
    float p = phase - floorf(phase);
    return _sine[(int)(p * 1024.0f) & 1023];
}

void AudioEngine::startVoice(uint8_t pad, uint8_t vel)
{
    PadSlot& p = pads[pad];
    if (p.kind == 0 || p.kind == 3) return;

    Voice* slot = nullptr;
    for (auto& v : _v) {
        if (v.active && v.pad == pad) { slot = &v; break; }
    }
    if (!slot) {
        for (auto& v : _v) {
            if (!v.active) { slot = &v; break; }
        }
    }
    if (!slot) {
        float lowest = 2.0f;
        for (auto& v : _v) {
            if (v.env < lowest) { lowest = v.env; slot = &v; }
        }
    }
    if (!slot) return;

    float g = (vel / 127.0f) * 0.55f;
    if (p.kind == 2 && p.data) {
        slot->active = true;
        slot->pad    = pad;
        slot->kind   = 2;
        slot->data   = p.data;
        slot->frames = p.frames;
        slot->pos    = 0;
        slot->step   = p.step;
        slot->gain   = g;
        slot->env    = 1.0f;
        slot->envMul = 1.0f;
        slot->age    = 0;
    } else {
        slot->active = true;
        slot->pad    = pad;
        slot->kind   = 1;
        slot->gain   = g;
        initSynthVoice(*slot, p.synth, 1.0f);
    }
    _level[pad] = 127;
}

void AudioEngine::initSynthVoice(Voice& v, uint8_t synth, float vel)
{
    v.synth   = synth;
    v.env     = 1.0f;
    v.phase   = 0;
    v.age     = 0;
    v.incMul  = 0.9998f;
    v.incMin  = 0.0f;
    v.noise   = 0;
    v.hp      = 0;
    v.lp      = 0;
    v.envMul  = 0.9997f;

    switch (synth) {
        case 0:  // kick
            v.inc = 150.0f / AUDIO_RATE;
            v.incMin = 44.0f / AUDIO_RATE;
            v.incMul = 0.99955f;
            v.envMul = 0.99982f;
            break;
        case 1:  // snare
            v.inc = 190.0f / AUDIO_RATE;
            v.envMul = 0.99945f;
            break;
        case 2:  // clap
            v.inc = 900.0f / AUDIO_RATE;
            v.envMul = 0.99930f;
            break;
        case 3:  // closed hat
            v.inc = 6000.0f / AUDIO_RATE;
            v.envMul = 0.99800f;
            break;
        case 4:  // open hat
            v.inc = 6000.0f / AUDIO_RATE;
            v.envMul = 0.99920f;
            break;
        case 5:  // tom
            v.inc = 170.0f / AUDIO_RATE;
            v.incMin = 85.0f / AUDIO_RATE;
            v.incMul = 0.99958f;
            v.envMul = 0.99955f;
            break;
        case 6:  // rim
            v.inc = 820.0f / AUDIO_RATE;
            v.envMul = 0.99820f;
            break;
        default:  // shaker
            v.inc = 9000.0f / AUDIO_RATE;
            v.envMul = 0.99880f;
            break;
    }
}

void AudioEngine::sequencer()
{
    if (!_playing) return;

    _tickAcc += AUDIO_BLOCK;
    while (_tickAcc >= _samplesPerTick) {
        _tickAcc -= _samplesPerTick;

        const int t = (int)(_tick % 96);
        const int step = (t / 6) % 16;
        if ((t % 6) == 0) {
            _playhead = (uint8_t)step;
            bool anySolo = false;
            for (int i = 0; i < 8; ++i) anySolo |= trackSolo[i];
            (void)anySolo;
            for (int tr = 0; tr < 8; ++tr) {
                if (trackMute[tr]) continue;
                const Step& s = pattern[tr][step];
                if (s.on) trigger(trackPad[tr], s.vel);
            }
        }
        ++_tick;
    }
}

void AudioEngine::render(int16_t* out, int frames)
{
    float master = (_volume / 100.0f) * 0.9f;

    for (int n = 0; n < frames; ++n) {
        int32_t mix = 0;
        for (auto& v : _v) {
            if (!v.active) continue;
            float s = 0;

            if (v.kind == 2) {
                uint32_t i = (uint32_t)v.pos;
                if (i + 1 >= v.frames) { v.active = false; continue; }
                float frac = v.pos - (float)i;
                float a = v.data[i];
                float b = v.data[i + 1];
                s = (a + (b - a) * frac) * v.gain;
                v.pos += v.step;
                v.age++;
                if (v.pos >= (float)(v.frames - 1)) v.active = false;
            } else {
                uint32_t rng = v.age * 1664525u + 1013904223u;
                float noise = ((int32_t)(rng >> 9) / 8388608.0f) - 1.0f;
                float osc = sine(v.phase);
                v.lp += (noise - v.lp) * 0.35f;
                float hp = noise - v.lp;

                switch (v.synth) {
                    case 0:
                        s = osc * v.env + (v.age < 90 ? (1.0f - v.age / 90.0f) * 0.5f : 0.0f);
                        break;
                    case 1:
                        s = (noise * 0.75f + osc * 0.35f) * v.env;
                        break;
                    case 2:
                        s = noise * v.env * (0.55f + 0.45f * fabsf(osc));
                        break;
                    case 3:
                    case 4:
                        s = hp * v.env;
                        break;
                    case 5:
                        s = osc * v.env;
                        break;
                    case 6:
                        s = (noise * 0.5f + osc * 0.5f) * v.env;
                        break;
                    default:
                        s = hp * v.env * 0.8f;
                        break;
                }
                s *= v.gain;

                v.phase += v.inc;
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                v.inc = v.inc * v.incMul;
                if (v.inc < v.incMin) v.inc = v.incMin;
                v.env *= v.envMul;
                v.age++;
                if (v.env < 0.0008f) v.active = false;
            }

            mix += (int32_t)(s * 26000.0f);
        }

        mix = (int32_t)(mix * master);
        if (mix > 32767) mix = 32767;
        if (mix < -32768) mix = -32768;
        out[n * 2]     = (int16_t)mix;
        out[n * 2 + 1] = (int16_t)mix;
    }
}
