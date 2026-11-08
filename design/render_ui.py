# -*- coding: utf-8 -*-
"""Cardputer-Adv MPC — UI mockups (Gruvbox Dark, SD-aware).

Drawn on a real 240x135 canvas with an 8 px Silkscreen font and 1 px strokes,
then upscaled x4 with nearest-neighbour.  Run:  python design/render_ui.py
"""

import math
import os
import random

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")
FONTS = os.path.join(HERE, "fonts")

W, H = 240, 135
S = 4

# ---------------------------------------------------------------- Gruvbox Dark
BG      = (29, 32, 33)     # dark0_hard
BAR     = (40, 40, 40)     # dark0
PANEL   = (40, 40, 40)
PADBG   = (50, 48, 47)     # dark0_soft
EDGE    = (80, 73, 69)     # dark2
LINE    = (60, 56, 54)     # dark1
TXT     = (235, 219, 178)  # light1
DIM     = (168, 153, 132)  # light4
FAINT   = (124, 111, 100)  # dark4
ACC     = (254, 128, 25)   # bright orange
ACC_DIM = (74, 42, 23)
AMB     = (250, 189, 47)   # bright yellow
AQUA    = (142, 192, 124)  # bright aqua
BLUE    = (131, 165, 152)  # bright blue
RED     = (251, 73, 52)    # bright red
GREEN   = (184, 187, 38)   # bright green

f8  = ImageFont.truetype(os.path.join(FONTS, "Silkscreen-Regular.ttf"), 8)
f8b = ImageFont.truetype(os.path.join(FONTS, "Silkscreen-Bold.ttf"), 8)
f16 = ImageFont.truetype(os.path.join(FONTS, "Silkscreen-Bold.ttf"), 16)


def txt(d, x, y, s, font=f8, fill=TXT):
    off = -4 if font.size == 8 else -7
    d.text((x, y + off), s, font=font, fill=fill)


def tw(s, font=f8):
    return int(font.getlength(s))


def rect(d, x, y, w, h, fill=None, outline=None):
    d.rectangle([x, y, x + w - 1, y + h - 1], fill=fill, outline=outline)


def bar(d, x, y, w, h, ratio, color, bg=EDGE):
    rect(d, x, y, w, h, bg)
    fw = int((w - 2) * max(0.0, min(1.0, ratio)))
    if fw > 0:
        rect(d, x + 1, y + 1, fw, h - 2, color)


def chip(d, x, y, w, h, label, active=False, color=ACC, text_on=None, danger=False):
    if active:
        rect(d, x, y, w, h, color)
        txt(d, x + (w - tw(label)) // 2, y + 3, label, f8b, text_on or BG)
    else:
        rect(d, x, y, w, h, None, RED if danger else EDGE)
        txt(d, x + (w - tw(label)) // 2, y + 3, label, f8, RED if danger else DIM)


def battery(d, x, y, pct):
    rect(d, x, y, 12, 6, None, DIM)
    rect(d, x + 12, y + 2, 2, 2, DIM)
    n = 8 if pct > 70 else 5 if pct > 35 else 2
    rect(d, x + 1, y + 1, n, 4, GREEN if pct > 35 else RED)


def sd_tag(d, x, y, state="ok"):
    col = GREEN if state == "ok" else AMB if state == "busy" else RED
    txt(d, x, y, "SD", f8, col)


def topbar(d, active, bpm="92.0", pct=78, sd="ok"):
    rect(d, 0, 0, W, 12, BAR)
    d.line([0, 11, W, 11], fill=LINE)
    tabs = ["PAD", "SEQ", "SMP", "MIX", "SYS", "KIT"]
    x = 3
    for i, name in enumerate(tabs):
        if i == active:
            txt(d, x, 2, name, f8b, ACC)
            d.line([x, 10, x + tw(name, f8b) - 1, 10], fill=ACC)
        else:
            txt(d, x, 2, name, f8, FAINT)
        x += 24
    txt(d, 150, 2, bpm, f8b, TXT)
    txt(d, 178, 2, "BPM", f8, FAINT)
    sd_tag(d, 204, 2, sd)
    battery(d, 224, 3, pct)


def bottombar(d, s, color=FAINT):
    rect(d, 0, 124, W, 11, BAR)
    d.line([0, 124, W, 124], fill=LINE)
    txt(d, 3, 126, s, f8, color)


def screen():
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    return img, d


# ------------------------------------------------------------------ 01 PAD
# (name, playing, cache)  cache: R = preloaded in RAM, S = streamed, ! = missing
PADS = [
    ("KICK", 1, "R"), ("SNR", 0, "R"), ("CLAP", 0, "R"), ("HATC", 0, "R"),
    ("HATO", 0, "S"), ("TOML", 1, "R"), ("TOMH", 0, "S"), ("RIM", 0, "R"),
    ("SHAK", 0, "S"), ("COW", 0, "R"), ("PERC", 0, "R"), ("PER2", 0, "!"),
    ("FX1", 0, "S"), ("FX2", 0, "R"), ("VOX", 0, "S"), ("--", 0, None),
]


def pad_screen():
    img, d = screen()
    topbar(d, 0)

    for i, (name, playing, cache) in enumerate(PADS):
        r, c = divmod(i, 4)
        x = 4 + c * 30
        y = 15 + r * 27
        if playing:
            rect(d, x, y, 28, 25, (63, 42, 27), ACC)
            txt(d, x + 2, y + 3, name, f8b, (251, 241, 199))
            rect(d, x + 3, y + 20, 22, 2, ACC_DIM)
            rect(d, x + 3, y + 20, 14, 2, ACC)
        else:
            rect(d, x, y, 28, 25, PADBG, EDGE)
            txt(d, x + 2, y + 3, name, f8, DIM if name == "--" else TXT)
        if i == 0:
            d.rectangle([x - 1, y - 1, x + 28, y + 26], outline=(189, 174, 147))
        txt(d, x + 2, y + 14, str(i + 1), f8, ACC if playing else FAINT)
        if cache:
            col = {"R": DIM, "S": AQUA, "!": RED}[cache]
            txt(d, x + 21, y + 13, cache, f8b, col)

    px = 127
    txt(d, px, 15, "KICK_808", f8b, TXT)
    txt(d, px, 26, "A01 0.42S 22K RAM", f8, FAINT)
    d.line([px, 36, 236, 36], fill=LINE)

    rect(d, px, 39, 109, 16, (24, 26, 27), LINE)
    rnd = random.Random(7)
    for x in range(108):
        t = x / 108.0
        env = math.exp(-4.2 * t) * (0.55 + 0.45 * abs(math.sin(t * 34)))
        hh = int(env * 7 * (0.6 + 0.4 * rnd.random()))
        d.line([px + 1 + x, 47 - hh, px + 1 + x, 47 + hh], fill=(205, 108, 60))

    for i, (lab, v) in enumerate((("VOL", 0.82), ("TUNE", 0.5), ("CUT", 0.74), ("DEC", 0.38))):
        y = 60 + i * 14
        txt(d, px, y, lab, f8, DIM)
        bar(d, px + 32, y + 2, 76, 5, v, BLUE if i else ACC)

    chip(d, px, 112, 40, 11, "1-SHOT", True)
    chip(d, px + 44, 112, 34, 11, "CHOKE")
    chip(d, px + 82, 112, 24, 11, "16LV")

    bottombar(d, "FN+1-4 BANK  OPT 16LV  SPC PLAY")
    return img


# ------------------------------------------------------------------ 02 SEQ
TRACKS = ["KIK", "SNR", "CLP", "HAT", "OPN", "TOM", "RIM", "FX"]
PATTERN = [
    [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0],
    [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0],
    [1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0],
    [0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0],
]
VEL = [
    [127, 0, 0, 0, 96, 0, 0, 0, 118, 0, 0, 0, 110, 0, 62, 0],
    [0, 0, 0, 0, 112, 0, 0, 0, 0, 0, 0, 0, 108, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 90, 0, 0, 0],
    [88, 0, 54, 0, 92, 0, 48, 0, 86, 0, 52, 0, 96, 0, 60, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 78, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 84, 0, 0],
    [0, 0, 0, 96, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 70, 0, 0, 0, 0, 0, 0],
]


def vel_color(v):
    k = v / 127.0
    return (int(120 + 134 * k), int(58 + 70 * k), int(30 + -5 * k))


def seq_screen():
    img, d = screen()
    topbar(d, 1)
    txt(d, 3, 14, "PAT 01", f8b, ACC)
    txt(d, 52, 14, "BAR 1/4", f8, FAINT)
    txt(d, 118, 14, "SW 54", f8, FAINT)
    txt(d, 214, 14, "REC", f8, RED)
    rect(d, 232, 16, 4, 4, RED)

    x0, y0 = 25, 24
    ph = 6
    for r, name in enumerate(TRACKS):
        y = y0 + r * 12
        txt(d, 3, y + 1, name, f8, DIM)
        for c in range(16):
            x = x0 + c * 13
            beat = c % 4 == 0
            base = (60, 56, 54) if c == ph else ((44, 41, 40) if beat else (36, 34, 33))
            rect(d, x, y, 12, 9, base)
            if PATTERN[r][c]:
                rect(d, x + 1, y + 1, 10, 7, vel_color(VEL[r][c]))

    d.polygon([(x0 + ph * 13 + 2, 21), (x0 + ph * 13 + 9, 21), (x0 + ph * 13 + 5, 23)], fill=TXT)
    rect(d, x0 + ph * 13, y0 + 3 * 12, 12, 9, None, TXT)

    bottombar(d, "FN ARROWS MOVE  ENT TOGGLE  SPC PLAY")
    return img


# ------------------------------------------------------------------ 03 SAMPLE
def wave_points(n=231):
    pts = []
    phase = 0.0
    dt = 0.42 / (n - 1)
    for i in range(n):
        t = i * dt
        f = 44.0 + 150.0 * math.exp(-t * 42.0)
        phase += 2.0 * math.pi * f * dt
        env = math.exp(-t * 8.0)
        click = math.exp(-t * 520.0) * 0.55
        pts.append(max(-1.0, min(1.0, (math.sin(phase) * 0.95 + click) * env)))
    return pts


def sample_screen():
    img, d = screen()
    topbar(d, 2)
    txt(d, 3, 14, "KICK_808", f8b, TXT)
    txt(d, 58, 14, "RAM", f8, DIM)
    txt(d, 196, 14, "0.42S", f8, DIM)

    px, py, pw, ph = 3, 24, 233, 58
    rect(d, px, py, pw, ph, (24, 26, 27), LINE)
    cy = py + ph // 2
    d.line([px, cy, px + pw - 1, cy], fill=(60, 56, 54))

    pts = wave_points(pw - 2)
    sx, ex = 10, 212
    for i, s in enumerate(pts):
        h = int(abs(s) * (ph // 2 - 3))
        x = px + 1 + i
        col = (215, 118, 76) if sx <= x <= ex else (96, 62, 44)
        if h > 0:
            d.line([x, cy - h, x, cy + h], fill=col)

    d.line([sx, py, sx, py + ph - 1], fill=AQUA)
    d.line([ex, py, ex, py + ph - 1], fill=AQUA)
    d.polygon([(sx - 3, py), (sx + 3, py), (sx, py + 4)], fill=AQUA)
    d.polygon([(ex - 3, py), (ex + 3, py), (ex, py + 4)], fill=AQUA)

    for k in range(4):
        t = k * 0.1
        x = px + 1 + int((t / 0.42) * (pw - 2))
        d.line([x, py + ph - 4, x, py + ph - 1], fill=(96, 89, 82))
        txt(d, x + 2, py + ph - 9, f"{t:.1f}", f8, FAINT)

    txt(d, 3, 86, "ST 0.000", f8, DIM)
    txt(d, 62, 86, "END 0.420", f8, DIM)
    txt(d, 126, 86, "LEN 0.420", f8, DIM)
    txt(d, 196, 86, "22.05K", f8, FAINT)

    labels = [("NORM", False), ("REV", False), ("TRIM", False),
              ("SLICE 8", True), ("ZOOM", False)]
    widths = [tw(l) + 8 for l, _ in labels]
    gap = (233 - sum(widths)) // (len(labels) - 1)
    x = 3
    for (lab, act), w in zip(labels, widths):
        chip(d, x, 100, w, 12, lab, active=act)
        x += w + gap

    bottombar(d, "FN+,- TRIM  ENT PREVIEW  BKS BACK")
    return img


# ------------------------------------------------------------------ 04 MIX
MIX = [
    ("KIK", 0.82, 0.10, False, False),
    ("SNR", 0.74, 0.22, False, False),
    ("CLP", 0.48, 0.30, False, False),
    ("HAT", 0.61, 0.05, False, False),
    ("OPN", 0.44, 0.18, True, False),
    ("TOM", 0.52, 0.12, False, False),
    ("RIM", 0.28, 0.34, False, False),
    ("FX", 0.36, 0.46, False, True),
]


def mix_screen():
    img, d = screen()
    topbar(d, 3)
    txt(d, 3, 14, "TRK", f8, FAINT)
    txt(d, 52, 14, "LEVEL", f8, FAINT)
    txt(d, 168, 14, "FX SEND", f8, FAINT)

    for i, (name, lvl, send, mute, solo) in enumerate(MIX):
        y = 23 + i * 12
        txt(d, 3, y + 1, name, f8, TXT if not mute else FAINT)
        rect(d, 25, y, 9, 9, ACC_DIM if mute else None, EDGE)
        txt(d, 27, y + 1, "M", f8, ACC if mute else FAINT)
        rect(d, 37, y, 9, 9, (44, 56, 36) if solo else None, EDGE)
        txt(d, 39, y + 1, "S", f8, GREEN if solo else FAINT)
        bar(d, 50, y + 2, 104, 6, lvl, ACC if not mute else ACC_DIM)
        bar(d, 168, y + 2, 64, 6, send, AQUA)

    bottombar(d, "M/S MUTE SOLO  FN ARROWS  ENT SOLO")
    return img


# ------------------------------------------------------------------ 05 SYS
def sys_screen():
    img, d = screen()
    topbar(d, 4)
    txt(d, 4, 16, "BATT 78%", f8b, GREEN)
    bar(d, 4, 26, 100, 4, 0.78, GREEN)

    txt(d, 4, 36, "CPU", f8, DIM)
    txt(d, 34, 36, "34%", f8b, TXT)
    bar(d, 4, 45, 110, 4, 0.34, BLUE)

    txt(d, 4, 52, "MEM", f8, DIM)
    txt(d, 34, 52, "118K", f8b, TXT)
    bar(d, 4, 61, 110, 4, 0.62, AMB)

    txt(d, 4, 70, "VOICES 24", f8, DIM)
    txt(d, 4, 82, "PADS 16/16", f8, DIM)

    txt(d, 124, 16, "SD READY", f8b, GREEN)
    txt(d, 124, 28, "12.4G FREE", f8b, TXT)
    txt(d, 124, 38, "1.8 MB/S", f8, FAINT)
    txt(d, 124, 50, "STALL 0", f8, DIM)
    txt(d, 124, 62, "MIDI BLE OFF", f8, FAINT)
    txt(d, 124, 74, "FW 0.1.0", f8, FAINT)

    rect(d, 4, 96, 232, 24, (24, 26, 27), LINE)
    txt(d, 8, 105, "UNDERRUN", f8, DIM)
    txt(d, 62, 105, "0", f8b, GREEN)
    txt(d, 80, 105, "LAT 8.2MS", f8, FAINT)
    txt(d, 160, 105, "POOL 62%", f8, FAINT)

    bottombar(d, "ENT SD  BKS BACK  HOLD G0 RESET")
    return img


# ------------------------------------------------------------------ 06 KIT
KITS = [
    ("808 CORE", "16/16", "2.1M", "RAM"),
    ("909", "16/16", "1.8M", "RAM"),
    ("ACOUSTIC", "16/16", "3.4M", "STR"),
    ("LO-FI", "16/16", "2.6M", "STR"),
    ("USER 01", "12/16", "0.9M", "MISS"),
    ("EMPTY", "0/16", "-", ""),
]


def kits_screen():
    img, d = screen()
    topbar(d, 5)
    txt(d, 4, 15, "KITS", f8b, ACC)
    txt(d, 44, 15, "/KITS", f8, FAINT)
    txt(d, 176, 15, "FREE 12.4G", f8, FAINT)

    for i, (name, cnt, size, mode) in enumerate(KITS):
        y = 27 + i * 15
        sel = i == 4
        if sel:
            rect(d, 2, y - 2, 236, 14, (60, 56, 54))
            rect(d, 2, y - 2, 2, 14, ACC)
        txt(d, 8, y, name, f8b if sel else f8, ACC if sel else TXT)
        txt(d, 126, y, cnt, f8, DIM)
        txt(d, 166, y, size, f8, FAINT)
        if mode:
            col = {"RAM": DIM, "STR": AQUA, "MISS": RED}[mode]
            txt(d, 200, y, mode, f8b, col)

    bottombar(d, "ENT LOAD  BKS DEL  TAB BACK")
    return img


# ------------------------------------------------------------------ 08 SD
def sd_screen():
    img, d = screen()
    topbar(d, 4, sd="ok")
    txt(d, 4, 15, "SD CARD", f8b, TXT)
    txt(d, 204, 15, "READY", f8b, GREEN)

    bar(d, 4, 26, 232, 8, 0.58, ACC)
    txt(d, 4, 38, "USED 17.3G", f8, DIM)
    txt(d, 180, 38, "FREE 12.4G", f8, DIM)

    lines = [
        ("SDHC 32G  FAT32  CL10", TXT),
        ("READ 1.8 MB/S  WRITE 0.9", DIM),
        ("ERR 0   STALL 0   RETRY 0", DIM),
        ("BUF 62%   STREAM 2/2", AQUA),
        ("6 KITS  12 PROJ  34 REC", FAINT),
    ]
    y = 50
    for s, col in lines:
        txt(d, 4, y, s, f8, col)
        y += 10

    chip(d, 4, 104, 34, 12, "TEST")
    chip(d, 42, 104, 56, 12, "REMOUNT")
    chip(d, 102, 104, 50, 12, "FORMAT", danger=True)
    txt(d, 158, 107, "ERASES CARD", f8, FAINT)

    bottombar(d, "ENT TEST  FN+DEL FORMAT  BKS BACK")
    return img


# ------------------------------------------------------------------ 09 LOADING
def loading_screen():
    img, d = screen()
    rect(d, 0, 0, W, 12, BAR)
    d.line([0, 11, W, 11], fill=LINE)
    txt(d, 3, 2, "LOADING", f8b, ACC)
    sd_tag(d, 204, 2, "busy")

    txt(d, 12, 22, "808 CORE", f8b, TXT)
    txt(d, 12, 32, "READING KIT FROM SD", f8, FAINT)

    bar(d, 12, 46, 216, 8, 0.47, ACC)
    txt(d, 12, 60, "47%", f8b, ACC)
    txt(d, 44, 60, "1.0 / 2.1 MB", f8, DIM)
    txt(d, 170, 60, "1.6 MB/S", f8, FAINT)

    txt(d, 12, 74, "PRELOAD 6   STREAM 10", f8, DIM)
    txt(d, 170, 74, "4.2S LEFT", f8, FAINT)
    txt(d, 12, 90, "DO NOT REMOVE CARD", f8, AMB)

    bottombar(d, "BKS CANCEL")
    return img


# ------------------------------------------------------------------ 10 NO SD
def nosd_screen():
    img, d = screen()
    topbar(d, 0, sd="err")

    # card pictogram with a slash
    cx, cy = 100, 30
    rect(d, cx, cy, 40, 28, None, DIM)
    rect(d, cx + 4, cy + 4, 12, 4, DIM)
    d.line([cx + 2, cy + 2, cx + 37, cy + 25], fill=RED)
    d.line([cx + 37, cy + 2, cx + 2, cy + 25], fill=RED)

    s = "NO SD CARD"
    txt(d, (W - tw(s, f8b)) // 2, 66, s, f8b, RED)
    s = "INSERT A FAT32 CARD"
    txt(d, (W - tw(s)) // 2, 78, s, f8, DIM)
    s = "KITS AND SAMPLES LIVE ON SD"
    txt(d, (W - tw(s)) // 2, 90, s, f8, FAINT)
    s = "MOUNT FAILED  ERR 0X0D"
    txt(d, (W - tw(s)) // 2, 104, s, f8, (96, 89, 82))

    bottombar(d, "ENT RETRY   SYS DIAG")
    return img


# ------------------------------------------------------------------ 11 16 LEVELS
def levels_screen():
    img, d = screen()
    topbar(d, 0)

    for i in range(16):
        r, c = divmod(i, 4)
        x = 4 + c * 30
        y = 15 + r * 27
        k = i / 15.0
        vel = 8 + round(i * 119 / 15)
        bg = (46 + int(40 * k), 34 + int(16 * k), 24 + int(8 * k))
        rect(d, x, y, 28, 25, bg, ACC if i == 5 else EDGE)
        txt(d, x + 2, y + 2, str(i + 1), f8, FAINT)
        txt(d, x + 5, y + 10, str(vel), f8b, (251, 241, 199) if k > 0.35 else DIM)
        if i == 5:
            d.rectangle([x - 1, y - 1, x + 28, y + 26], outline=(189, 174, 147))

    px = 127
    txt(d, px, 15, "TOM_L", f8b, TXT)
    txt(d, px, 26, "16 LEVELS", f8, ACC)
    d.line([px, 36, 236, 36], fill=LINE)
    txt(d, px, 44, "VELOCITY", f8, DIM)
    txt(d, px, 54, "48", f8b, ACC)
    for i in range(16):
        on = i < 6
        rect(d, px + i * 6, 66, 5, 7, ACC if on else (60, 56, 54))
    txt(d, px, 82, "RANGE 8-127", f8, FAINT)
    txt(d, px, 94, "OPT HELD", f8, ACC)
    txt(d, px, 106, "PAD 06", f8, FAINT)

    bottombar(d, "TAP PAD SET VEL  RELEASE OPT EXIT")
    return img


# ------------------------------------------------------------------ export
def save(img, name, scale=S):
    img.resize((W * scale, H * scale), Image.NEAREST).save(os.path.join(OUT, name))


def contact_sheet(tiles, out_name, cols, title, subtitle):
    pad = 18
    lab = 22
    tile_w, tile_h = W * 2, H * 2
    rows = (len(tiles) + cols - 1) // cols
    cw = cols * tile_w + (cols + 1) * pad
    ch = 74 + rows * (tile_h + lab) + (rows - 1) * pad + 16

    sheet = Image.new("RGB", (cw, ch), (24, 26, 27))
    d = ImageDraw.Draw(sheet)
    d.fontmode = "1"
    txt(d, pad, 18, title, f16, TXT)
    txt(d, pad, 40, subtitle, f8, FAINT)

    for i, (name, label) in enumerate(tiles):
        r, c = divmod(i, cols)
        x = pad + c * (tile_w + pad)
        y = 62 + r * (tile_h + lab + pad)
        img = Image.open(os.path.join(OUT, name)).resize((tile_w, tile_h), Image.NEAREST)
        sheet.paste(img, (x, y))
        d.rectangle([x - 1, y - 1, x + tile_w, y + tile_h], outline=(80, 73, 69))
        txt(d, x, y + tile_h + 6, label, f8, DIM)

    sheet.save(os.path.join(OUT, out_name))


def main():
    os.makedirs(OUT, exist_ok=True)
    screens = {
        "01-pad.png": pad_screen(),
        "02-seq.png": seq_screen(),
        "03-sample.png": sample_screen(),
        "04-mix.png": mix_screen(),
        "05-sys.png": sys_screen(),
        "06-kits.png": kits_screen(),
        "07-levels.png": levels_screen(),
        "08-sd.png": sd_screen(),
        "09-loading.png": loading_screen(),
        "10-nosd.png": nosd_screen(),
    }
    for name, img in screens.items():
        save(img, name)

    contact_sheet([
        ("01-pad.png", "01 PAD / PERFORM"),
        ("02-seq.png", "02 SEQ / STEPS"),
        ("03-sample.png", "03 SAMPLE / TRIM"),
        ("04-mix.png", "04 MIX / LEVELS"),
        ("05-sys.png", "05 SYS / DIAGNOSTICS"),
        ("06-kits.png", "06 KITS / SD-AWARE"),
    ], "00-overview.png", 3,
        "CARDPUTER-ADV MPC  /  GRUVBOX DARK",
        "240x135 PIXEL GRID / SILKSCREEN 8PX / 1PX STROKES / SD-AWARE")

    contact_sheet([
        ("06-kits.png", "06 KITS / RAM vs STREAM"),
        ("08-sd.png", "08 SD / CARD HEALTH"),
        ("09-loading.png", "09 LOADING / KIT"),
        ("10-nosd.png", "10 NO SD / RECOVERY"),
    ], "00-sd-states.png", 2,
        "SD STATES",
        "CARDS FAIL, FILL UP AND GO SLOW: THE UI HAS TO SAY SO BEFORE THE AUDIO DOES")

    print("rendered to", OUT)


if __name__ == "__main__":
    main()
