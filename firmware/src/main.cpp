#include <Arduino.h>
#include <M5Cardputer.h>
#include <Preferences.h>

#include "app.h"
#include "audio.h"
#include "samples.h"
#include "ui.h"

AppState app;
static Preferences prefs;

static const char* PAD_KEYS[16] = {
    "1", "2", "3", "4",
    "q", "w", "e", "r",
    "a", "s", "d", "f",
    "z", "x", "c", "v"};

void setStatus(const char* s, uint32_t ms)
{
    strncpy(app.status, s, sizeof(app.status) - 1);
    app.statusUntil = millis() + ms;
}

static void loadProgress(int pct, const char* name)
{
    app.loadingPct = pct;
    if (name && name[0]) {
        char bn[20];
        const char* slash = strrchr(name, '/');
        const char* p = slash ? slash + 1 : name;
        size_t i = 0;
        while (p[i] && p[i] != '.' && i + 1 < sizeof(bn)) { bn[i] = p[i]; ++i; }
        bn[i] = 0;
        strncpy(app.loadingName, bn, sizeof(app.loadingName) - 1);
    }
    ui.renderLoading(pct, app.loadingName);
}

static void doLoadKit(int idx)
{
    if (idx < 0 || idx >= app.kitCount) return;
    strncpy(app.loadingName, app.kits[idx].name, sizeof(app.loadingName) - 1);
    app.page = Page::LOADING;
    app.loadingPct = 0;
    ui.renderLoading(0, app.loadingName);
    bool ok = store.loadKit(idx, loadProgress);
    if (!ok) {
        store.loadSynthKit();
        setStatus("LOAD FAILED - SYNTH KIT");
    } else {
        setStatus("KIT LOADED");
    }
    app.page = Page::PAD;
}

static void mountSd(bool boot)
{
    app.sd = SdState::MOUNTING;
    if (!store.begin()) {
        app.sd = SdState::NONE;
        strncpy(app.sdError, "MOUNT FAILED  ERR 0X0D", sizeof(app.sdError) - 1);
        store.loadSynthKit();
        if (boot) app.page = Page::NOSD;
        return;
    }
    app.sd = SdState::READY;
    app.sdTotalMB = store.totalMB();
    app.sdFreeMB  = store.freeMB();
    app.sdReadMBs = store.testRead();
    store.scanKits();
    Serial.printf("[mpc] sd=%s total=%uM free=%uM read=%.2fMB/s kits=%d\n",
                  store.cardName(), (unsigned)app.sdTotalMB, (unsigned)app.sdFreeMB,
                  app.sdReadMBs, app.kitCount);
    if (app.kitCount > 0) {
        app.kitSel = 0;
        doLoadKit(0);
    } else {
        store.loadSynthKit();
        setStatus("NO KITS - SYNTH KIT");
        app.page = Page::PAD;
    }
}

static void nextPage()
{
    switch (app.page) {
        case Page::PAD: app.page = Page::SEQ; break;
        case Page::SEQ: app.page = Page::SMP; break;
        case Page::SMP: app.page = Page::MIX; break;
        case Page::MIX: app.page = Page::SYS; break;
        case Page::SYS: app.page = Page::KIT; break;
        case Page::KIT: app.page = Page::PAD; break;
        default: app.page = Page::PAD; break;
    }
}

static void startSampling()
{
    if (app.recording) return;
    uint32_t freeSamples = audio.poolFree();
    uint32_t cap = 22050 * 2;  // 2 s at 22.05 kHz
    if (cap > freeSamples) cap = freeSamples;
    cap &= ~1u;
    if (cap < 2205) {
        setStatus("POOL FULL");
        return;
    }

    int16_t* buf = audio.allocRecord(cap);
    if (!buf) {
        setStatus("NO POOL");
        return;
    }

    audio.setPlaying(false);
    app.playing = false;
    if (!audio.startRecord(buf, cap)) {
        audio.freeLast(buf, cap);
        setStatus("REC BUSY");
        return;
    }
    app.recBuf   = buf;
    app.recMax   = cap;
    app.recPos   = 0;
    app.recording = true;
}

static void finalizeRecording()
{
    bool ok = app.recPos > 200;
    if (ok) {
        PadSlot& p = audio.pads[app.selected];
        if (p.kind == 2 && p.data) audio.freeLast(p.data, p.frames);
        memset(&p, 0, sizeof(p));
        p.kind   = 2;
        p.data   = app.recBuf;
        p.frames = app.recPos;
        p.step   = 22050.0f / AUDIO_RATE;

        char nm[10] = "REC";
        int idx = store.saveRecording(p, nm, sizeof(nm));
        strncpy(p.name, nm, sizeof(p.name) - 1);
        if (idx > 0) {
            store.scanKits();
            if (app.kitSel >= app.kitCount) app.kitSel = 0;
        }
        setStatus(idx > 0 ? "SAMPLED + SAVED" : "SAMPLED (NO SD)");
    } else {
        audio.freeLast(app.recBuf, app.recMax);
        setStatus("REC FAILED");
    }
    app.recording = false;
    app.recBuf = nullptr;
}

static void updateRecording()
{
    if (!app.recording) return;
    app.recPos = audio.recordPos();
    uint8_t st = audio.recordState();
    if (st == AudioEngine::REC_DONE) {
        finalizeRecording();
    } else if (st == AudioEngine::REC_IDLE) {
        audio.freeLast(app.recBuf, app.recMax);
        app.recording = false;
        app.recBuf = nullptr;
        setStatus(app.recPos > 200 ? "CANCELLED" : "REC FAILED");
    }
}

static void onEnter()
{
    switch (app.page) {
        case Page::PAD:
            startSampling();
            break;
        case Page::SEQ:
            audio.pattern[app.cursorTrack][app.cursorStep].on ^= 1;
            break;
        case Page::MIX:
            audio.trackMute[app.cursorTrack] ^= 1;
            break;
        case Page::SYS:
            if (app.sd == SdState::READY) app.page = Page::SD;
            break;
        case Page::SD: {
            float s = store.testRead();
            app.sdReadMBs = s;
            setStatus(s > 0 ? "CARD OK" : "CARD TEST FAILED");
            break;
        }
        case Page::KIT:
            doLoadKit(app.kitSel);
            break;
        case Page::NOSD:
            mountSd(true);
            break;
        default:
            break;
    }
}

static void onBack()
{
    switch (app.page) {
        case Page::SEQ:
            audio.pattern[app.cursorTrack][app.cursorStep].on = 0;
            break;
        case Page::SD:
            app.page = Page::SYS;
            break;
        case Page::KIT:
            app.page = Page::PAD;
            break;
        case Page::SMP:
            app.page = Page::PAD;
            break;
        default:
            break;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(30);
    Serial.printf("\n[mpc] boot, heap=%u\n", (unsigned)ESP.getFreeHeap());

    auto cfg = M5.config();
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    M5Cardputer.begin(cfg, true);
    // M5GFX 0.3.0 already boots Cardputer/ADV at rotation 0 (240x135).
    // setRotation(1) would swap the axes to 135x240 and clip the UI.
    M5Cardputer.Display.setBrightness(140);

    prefs.begin("mpc", false);
    app.volume = prefs.getUChar("vol", 78);
    app.bpm    = prefs.getUChar("bpm", 92);
    app.flip   = prefs.getBool("flip", false);
    M5Cardputer.Display.setRotation(app.flip ? 2 : 0);
    Serial.printf("[mpc] display %dx%d rot=%d\n",
                  M5Cardputer.Display.width(), M5Cardputer.Display.height(),
                  M5Cardputer.Display.getRotation());

    // boot splash on the raw display: proves panel + backlight + orientation
    // before the sprite/audio stack comes up.
    M5Cardputer.Display.fillScreen(0x0000);
    M5Cardputer.Display.setTextColor(0xFD20);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setCursor(10, 12);
    M5Cardputer.Display.print("MPC");
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(0xE71C);
    M5Cardputer.Display.setCursor(10, 44);
    M5Cardputer.Display.printf("%dx%d  rot=%d", M5Cardputer.Display.width(),
                               M5Cardputer.Display.height(), M5Cardputer.Display.getRotation());
    M5Cardputer.Display.setCursor(10, 62);
    M5Cardputer.Display.print("FN+R = FLIP");
    delay(350);

    ui.begin();
    audio.begin();
    audio.setVolume(app.volume);
    audio.setBpm(app.bpm);
    audio.setSwing(app.swing);
    audio.start();

    app.codecOk = audio.codecOk();
    Serial.printf("[mpc] codec=%d\n", app.codecOk ? 1 : 0);
    app.page = Page::LOADING;
    app.loadingPct = 0;
    strcpy(app.loadingName, "STARTING");
    ui.renderLoading(0, "STARTING");

    mountSd(true);

    app.battery = M5.Power.getBatteryLevel();
    if (app.battery < 0) app.battery = 78;

    if (app.page == Page::PAD) setStatus(app.codecOk ? "READY" : "NO CODEC");
}

void loop()
{
    M5Cardputer.update();
    auto& ks = M5Cardputer.Keyboard.keysState();

    static bool prevPad[16] = {false};
    static bool prevTab = false, prevSpace = false, prevEnt = false, prevDel = false;
    static bool prevBankKey[4] = {false};
    static bool prevFlip = false;
    static uint32_t lastRender = 0, lastBat = 0;

    app.levelsMode = ks.opt;

    if (app.recording) {
        app.recPos = audio.recordPos();
        if (ks.del && !prevDel) audio.cancelRecord();
        prevDel = ks.del;
        updateRecording();
        if (millis() - lastRender >= 33) {
            lastRender = millis();
            ui.render();
        }
        return;
    }

    for (int i = 0; i < 16; ++i) {
        bool down = M5Cardputer.Keyboard.isKeyPressed(PAD_KEYS[i][0]);
        if (down && !prevPad[i]) {
            app.selected = i;
            if (app.levelsMode) {
                app.levelValue = (uint8_t)(8 + i * 119 / 15);
            } else if (!ks.fn) {
                audio.trigger(i, 112);
            }
        } else if (!down && prevPad[i]) {
            audio.release(i);
        }
        prevPad[i] = down;
    }

    if (ks.tab && !prevTab) nextPage();
    prevTab = ks.tab;

    if (ks.space && !prevSpace) {
        app.playing = !app.playing;
        audio.setPlaying(app.playing);
    }
    prevSpace = ks.space;

    if (ks.enter && !prevEnt) onEnter();
    prevEnt = ks.enter;

    if (ks.del && !prevDel) onBack();
    prevDel = ks.del;

    if (ks.fn) {
        const char* bk[4] = {"1", "2", "3", "4"};
        for (int i = 0; i < 4; ++i) {
            bool down = M5Cardputer.Keyboard.isKeyPressed((char)bk[i][0]);
            if (down && !prevBankKey[i]) {
                app.bank = i;
                setStatus("BANK A");
            }
            prevBankKey[i] = down;
        }

        bool r = M5Cardputer.Keyboard.isKeyPressed('r');
        if (r && !prevFlip) {
            app.flip = !app.flip;
            M5Cardputer.Display.setRotation(app.flip ? 2 : 0);
            prefs.putBool("flip", app.flip);
            setStatus(app.flip ? "SCREEN 180" : "SCREEN 0");
        }
        prevFlip = r;
    }

    // Fn + ; . , / = arrows
    static bool prevUp = false, prevDown = false, prevLeft = false, prevRight = false;
    if (ks.fn) {
        bool up    = M5Cardputer.Keyboard.isKeyPressed(';');
        bool down  = M5Cardputer.Keyboard.isKeyPressed('.');
        bool left  = M5Cardputer.Keyboard.isKeyPressed(',');
        bool right = M5Cardputer.Keyboard.isKeyPressed('/');

        int dy = (down && !prevDown) - (up && !prevUp);
        int dx = (right && !prevRight) - (left && !prevLeft);

        if (app.page == Page::SEQ) {
            app.cursorTrack = (uint8_t)((app.cursorTrack + dy + 8) % 8);
            app.cursorStep  = (uint8_t)((app.cursorStep + dx + 16) % 16);
        } else if (app.page == Page::MIX) {
            app.cursorTrack = (uint8_t)((app.cursorTrack + dy + 8) % 8);
        } else if (app.page == Page::KIT) {
            if (dy && app.kitCount) app.kitSel = (app.kitSel + dy + app.kitCount) % app.kitCount;
        } else if (app.page == Page::SMP) {
            app.selected = (uint8_t)((app.selected + dx + 16) % 16);
        }

        prevUp = up; prevDown = down;
        prevLeft = left; prevRight = right;
    }

    static bool prevMinus = false, prevPlus = false;
    bool minus = M5Cardputer.Keyboard.isKeyPressed('-');
    bool plus  = M5Cardputer.Keyboard.isKeyPressed('=');
    if (ks.fn) {
        if (minus && !prevMinus && app.bpm > 40) {
            app.bpm -= 1;
            audio.setBpm(app.bpm);
            prefs.putUChar("bpm", app.bpm);
            setStatus("BPM");
        }
        if (plus && !prevPlus && app.bpm < 240) {
            app.bpm += 1;
            audio.setBpm(app.bpm);
            prefs.putUChar("bpm", app.bpm);
            setStatus("BPM");
        }
    } else {
        if (minus && !prevMinus && app.volume >= 5) {
            app.volume -= 5;
            audio.setVolume(app.volume);
            prefs.putUChar("vol", app.volume);
            setStatus("VOLUME");
        }
        if (plus && !prevPlus && app.volume <= 95) {
            app.volume += 5;
            audio.setVolume(app.volume);
            prefs.putUChar("vol", app.volume);
            setStatus("VOLUME");
        }
    }
    prevMinus = minus; prevPlus = plus;

    if (millis() - lastBat > 1000) {
        lastBat = millis();
        int b = M5.Power.getBatteryLevel();
        if (b >= 0) app.battery = b;
        app.underruns = audio.underruns();
        app.cpu = audio.cpuPct();
        app.bufPct = audio.bufPct();
        if (app.sd == SdState::READY) app.sdFreeMB = store.freeMB();
    }

    if (millis() - lastRender >= 33) {
        lastRender = millis();
        ui.render();
    }
}
