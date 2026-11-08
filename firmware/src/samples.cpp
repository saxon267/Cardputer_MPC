#include "samples.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "app.h"
#include "audio.h"

SampleStore store;

static SPIClass sdSPI(HSPI);

static const char* SYNTH_NAMES[16] = {
    "KICK", "SNR", "CLAP", "HATC", "HATO", "TOML", "TOMH", "RIM",
    "SHAK", "COW", "PERC", "PER2", "FX1", "FX2", "VOX", "--"};

static uint8_t synthFor(uint8_t pad)
{
    static const uint8_t map[16] = {0, 1, 2, 3, 4, 5, 5, 6, 7, 6, 5, 2, 7, 3, 4, 0};
    return map[pad];
}

bool SampleStore::begin()
{
    sdSPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS, sdSPI, 20000000)) {
        _ready = false;
        strcpy(_cardName, "NONE");
        return false;
    }
    _ready = true;

    switch (SD.cardType()) {
        case CARD_MMC:  strcpy(_cardName, "MMC");  break;
        case CARD_SD:   strcpy(_cardName, "SDSC"); break;
        case CARD_SDHC: strcpy(_cardName, "SDHC"); break;
        default:        strcpy(_cardName, "SD");   break;
    }

    uint64_t total = SD.totalBytes();
    uint64_t used  = SD.usedBytes();
    _totalMB = (uint32_t)(total / (1024 * 1024));
    _freeMB  = (uint32_t)((total - used) / (1024 * 1024));
    return true;
}

static void baseName(const char* path, char* out, size_t n)
{
    const char* slash = strrchr(path, '/');
    const char* p = slash ? slash + 1 : path;
    size_t i = 0;
    while (p[i] && p[i] != '.' && i + 1 < n) {
        char c = p[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[i] = c;
        ++i;
    }
    out[i] = 0;
}

int SampleStore::scanKits()
{
    app.kitCount = 0;
    if (!_ready) return 0;

    File root = SD.open("/KITS");
    if (root && root.isDirectory()) {
        File e = root.openNextFile();
        while (e && app.kitCount < 8) {
            if (e.isDirectory()) {
                uint16_t count = 0;
                uint32_t bytes = 0;
                File d = SD.open(e.name());
                if (!d && e.name()[0] != '/') {
                    char p[64];
                    snprintf(p, sizeof(p), "/KITS/%s", e.name());
                    d = SD.open(p);
                }
                if (d) {
                    File f = d.openNextFile();
                    while (f) {
                        const char* nm = f.name();
                        size_t l = strlen(nm);
                        if (l > 4 && strcasecmp(nm + l - 4, ".wav") == 0) {
                            ++count;
                            bytes += f.size();
                        }
                        f.close();
                        f = d.openNextFile();
                    }
                    d.close();
                }
                if (count > 0) {
                    KitInfo& k = app.kits[app.kitCount];
                    char bn[20];
                    baseName(e.name(), bn, sizeof(bn));
                    strncpy(k.name, bn, sizeof(k.name) - 1);
                    k.samples = count;
                    k.bytes   = bytes;
                    ++app.kitCount;
                }
            }
            e.close();
            e = root.openNextFile();
        }
        root.close();
        return app.kitCount;
    }

    // flat /KITS/*.wav fallback
    root = SD.open("/KITS");
    if (!root) root = SD.open("/");
    if (root && root.isDirectory()) {
        uint16_t count = 0;
        uint32_t bytes = 0;
        File f = root.openNextFile();
        while (f) {
            const char* nm = f.name();
            size_t l = strlen(nm);
            if (l > 4 && strcasecmp(nm + l - 4, ".wav") == 0) {
                ++count;
                bytes += f.size();
            }
            f.close();
            f = root.openNextFile();
        }
        root.close();
        if (count) {
            KitInfo& k = app.kits[0];
            strncpy(k.name, "SD KIT", sizeof(k.name) - 1);
            k.samples = count;
            k.bytes   = bytes;
            app.kitCount = 1;
        }
    }
    return app.kitCount;
}

void SampleStore::loadSynthKit()
{
    audio.resetPool();
    for (int i = 0; i < 16; ++i) {
        PadSlot& p = audio.pads[i];
        memset(&p, 0, sizeof(p));
        strncpy(p.name, SYNTH_NAMES[i], sizeof(p.name) - 1);
        if (i == 15) {
            p.kind = 0;
        } else {
            p.kind  = 1;
            p.synth = synthFor(i);
        }
    }
    app.sampleBytes = 0;
}

bool SampleStore::loadWav(const char* path, uint8_t pad, uint32_t* bytesOut)
{
    File f = SD.open(path);
    if (!f || f.isDirectory()) return false;

    char riff[12];
    if (f.read((uint8_t*)riff, 12) != 12 || memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        f.close();
        return false;
    }

    uint16_t channels = 0, bits = 0;
    uint32_t rate = 0, dataPos = 0, dataSize = 0;
    uint32_t pos = 12;

    while (pos + 8 <= f.size()) {
        char id[4];
        uint32_t sz = 0;
        f.read((uint8_t*)id, 4);
        f.read((uint8_t*)&sz, 4);
        uint32_t chunkStart = f.position();
        if (memcmp(id, "fmt ", 4) == 0) {
            uint8_t fmt[16] = {0};
            f.read(fmt, sz < 16 ? sz : 16);
            memcpy(&channels, fmt + 2, 2);
            memcpy(&rate, fmt + 4, 4);
            memcpy(&bits, fmt + 14, 2);
            f.seek(chunkStart + sz + (sz & 1), SeekSet);
        } else if (memcmp(id, "data", 4) == 0) {
            dataPos  = chunkStart;
            dataSize = sz;
            break;
        } else {
            f.seek(chunkStart + sz + (sz & 1), SeekSet);
        }
        pos = f.position();
    }

    if (channels < 1 || bits != 16 || rate < 8000 || dataSize == 0) {
        f.close();
        return false;
    }

    uint32_t blockAlign = channels * 2;
    uint32_t frames = dataSize / blockAlign;
    uint32_t maxFrames = AUDIO_RATE * 4;  // cap 4 s per sample
    if (frames > maxFrames) frames = maxFrames;

    int16_t* dst = audio.alloc(frames);
    if (!dst) {
        f.close();
        return false;
    }

    f.seek(dataPos);
    const int CH = 256;
    int16_t buf[CH * 2];
    uint32_t done = 0;
    while (done < frames) {
        uint32_t n = frames - done;
        if (n > CH) n = CH;
        size_t want = n * blockAlign;
        size_t got = f.read((uint8_t*)buf, want);
        if (got < want) break;
        for (uint32_t i = 0; i < n; ++i) {
            int32_t s;
            if (channels == 1) {
                s = buf[i];
            } else {
                s = ((int32_t)buf[i * 2] + buf[i * 2 + 1]) / 2;
            }
            dst[done + i] = (int16_t)s;
        }
        done += n;
    }
    f.close();

    PadSlot& p = audio.pads[pad];
    memset(&p, 0, sizeof(p));
    baseName(path, p.name, sizeof(p.name));
    p.kind   = 2;
    p.data   = dst;
    p.frames = done;
    p.step   = (float)rate / AUDIO_RATE;
    *bytesOut = done * 2;
    return done > 0;
}

bool SampleStore::loadKit(int index, void (*progress)(int pct, const char* name))
{
    if (!_ready || index < 0 || index >= app.kitCount) return false;
    audio.resetPool();
    for (int i = 0; i < 16; ++i) memset(&audio.pads[i], 0, sizeof(PadSlot));

    const KitInfo& k = app.kits[index];
    char dirPath[64];
    snprintf(dirPath, sizeof(dirPath), "/KITS/%s", k.name);

    File dir = SD.open(dirPath);
    bool flat = false;
    if (!dir || !dir.isDirectory()) {
        dir  = SD.open("/KITS");
        flat = true;
    }
    if (!dir) return false;

    uint8_t pad = 0;
    uint32_t totalBytes = 0;
    File f = dir.openNextFile();
    while (f && pad < 16) {
        const char* nm = f.name();
        size_t l = strlen(nm);
        if (!f.isDirectory() && l > 4 && strcasecmp(nm + l - 4, ".wav") == 0) {
            char path[96];
            if (flat) snprintf(path, sizeof(path), "/KITS/%s", nm);
            else      snprintf(path, sizeof(path), "%s/%s", dirPath, nm);

            uint32_t bytes = 0;
            if (loadWav(path, pad, &bytes)) {
                totalBytes += bytes;
            } else {
                PadSlot& p = audio.pads[pad];
                memset(&p, 0, sizeof(p));
                baseName(nm, p.name, sizeof(p.name));
                p.kind = 3;
            }
            ++pad;
            if (progress) progress(pad * 100 / 16, f.name());
        }
        f.close();
        f = dir.openNextFile();
    }
    dir.close();

    app.sampleBytes = totalBytes;
    if (progress) progress(100, "DONE");
    return true;
}

float SampleStore::testRead()
{
    if (!_ready) return 0;

    File root = SD.open("/KITS");
    File f;
    if (root) {
        File e = root.openNextFile();
        while (e) {
            if (!e.isDirectory()) { f = SD.open(e.name()); break; }
            e.close();
            e = root.openNextFile();
        }
    }
    if (!f) f = SD.open("/README.TXT");
    if (!f) return 0;

    static uint8_t buf[4096];
    uint32_t total = 0;
    uint32_t t0 = micros();
    for (int i = 0; i < 32; ++i) {
        size_t n = f.read(buf, sizeof(buf));
        if (!n) break;
        total += n;
    }
    uint32_t dt = micros() - t0;
    f.close();
    if (dt == 0) return 0;
    _readMBs = (total / 1048576.0f) / (dt / 1000000.0f);
    return _readMBs;
}
