# -*- coding: utf-8 -*-
"""Generate the one-page A4 cheat sheet (docs/cheatsheet.png)."""

import os

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "docs", "cheatsheet.png")

W, H = 1240, 1754  # A4 @ 150 dpi
BG = (29, 32, 33)
PANEL = (40, 40, 40)
PANEL2 = (50, 48, 47)
EDGE = (80, 73, 69)
TXT = (235, 219, 178)
DIM = (168, 153, 132)
FAINT = (124, 111, 100)
ACC = (254, 128, 25)
AQUA = (142, 192, 124)
RED = (251, 73, 52)
GREEN = (184, 187, 38)
YEL = (250, 189, 47)

MS = "C:/Windows/Fonts/msyh.ttc"
MSB = "C:/Windows/Fonts/msyhbd.ttc"
SILK = os.path.join(ROOT, "design", "fonts", "Silkscreen-Regular.ttf")

f_title = ImageFont.truetype(SILK, 32)
f_sub = ImageFont.truetype(MS, 16)
f_h = ImageFont.truetype(MSB, 19)
f_body = ImageFont.truetype(MS, 16)
f_small = ImageFont.truetype(MS, 13)


def wrap(d, text, font, maxw):
    out, line = [], ""
    for ch in text:
        if ch == "\n":
            out.append(line)
            line = ""
            continue
        if d.textlength(line + ch, font=font) <= maxw:
            line += ch
        else:
            out.append(line)
            line = ch
    if line:
        out.append(line)
    return out


def section(d, x, y, w, num, title, lines, accent=ACC):
    d.rectangle([x, y, x + w - 1, y + 32], fill=PANEL, outline=EDGE)
    d.text((x + 10, y + 5), num, font=f_h, fill=accent)
    d.text((x + 48, y + 5), title, font=f_h, fill=TXT)

    cy = y + 44
    for text in lines:
        for ln in wrap(d, text, f_body, w - 20):
            d.text((x + 10, cy), ln, font=f_body, fill=TXT)
            cy += 24
        cy += 2
    d.rectangle([x, y, x + w - 1, cy + 6], outline=EDGE)
    return cy + 6


def box(d, x, y, w, h, fill, edge):
    d.rectangle([x, y, x + w - 1, y + h - 1], fill=fill, outline=edge)


def main():
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    # header
    d.rectangle([0, 0, W, 104], fill=(40, 40, 40))
    d.rectangle([0, 102, W, 104], fill=ACC)
    d.text((48, 20), "CARDPUTER_MPC", font=f_title, fill=TXT)
    d.text((50, 66), "CHEAT SHEET  v0.4   ·   M5Stack Cardputer-Adv   ·   240×135   ·   Gruvbox Dark",
           font=f_sub, fill=DIM)
    d.text((W - 250, 38), "MIC → PAD → SD", font=f_h, fill=ACC)

    # quick start
    d.text((48, 122), "60 秒上手", font=f_h, fill=ACC)
    qs = [
        ("1", "插卡", "FAT32，目录 /KITS 与 /REC"),
        ("2", "选垫", "PAD 页点一个垫"),
        ("3", "按 Ent", "对着麦克风，最长 2 s"),
        ("4", "自动", "声音上垫 + 写 /REC/REC_0001.WAV"),
    ]
    bw = (W - 96 - 3 * 16) // 4
    for i, (num, t, s) in enumerate(qs):
        x = 48 + i * (bw + 16)
        box(d, x, 152, bw, 96, PANEL2, EDGE)
        d.ellipse([x + 12, 164, x + 40, 192], fill=ACC)
        d.text((x + 21, 168), num, font=f_h, fill=BG)
        d.text((x + 50, 164), t, font=f_h, fill=TXT)
        d.multiline_text((x + 12, 204), "\n".join(wrap(d, s, f_small, bw - 24)),
                         font=f_small, fill=DIM, spacing=4)

    col_w = 550
    x1, x2 = 48, 48 + col_w + 44
    y1 = y2 = 276

    y1 = section(d, x1, y1, col_w, "01", "TRANSPORT / PAGES", [
        "Space   播放 / 停止",
        "Tab      下一页：PAD → SEQ → SMP → MIX → SYS → KIT",
        "Ent      上下文：PAD 采样 / SEQ 开关步 / KIT 载入",
        "Bksp    返回、清步、取消录音",
        "Fn + ; , . /   方向键（SEQ 光标、KIT 选择）",
        "Fn + 1..4   bank A–D",
        "Fn + R      屏幕翻转 180°（存 NVS）",
    ]) + 16

    y1 = section(d, x1, y1, col_w, "02", "PADS / 16 LEVELS", [
        "1 2 3 4 / Q W E R / A S D F / Z X C V = 1–16",
        "按住 Opt = 16 Levels（垫变成 1–16 级力度）",
        "Fn + - / =  BPM；- / =  音量",
        "每个垫右下角：R 内存 / S 流式 / ! 缺失",
        "空垫显示 --；按了没声先看采样池",
    ]) + 16

    y1 = section(d, x1, y1, col_w, "03", "SAMPLING（重点）", [
        "1. PAD 页选中目标垫",
        "2. 按 Ent 开始录麦克风（最长 2 s，22.05 kHz）",
        "3. 进度走完：声音上垫 + 自动写 /REC/REC_0001.WAV",
        "4. 录音中按 Bksp 取消",
        "5. 没有 SD 时只留内存，重启丢失",
        "导入：KIT → RECORDINGS → Ent 载回 16 个垫",
    ], accent=RED) + 16

    y1 = section(d, x1, y1, col_w, "04", "SD / WAV", [
        "/REC/REC_0001.WAV   录音",
        "/KITS/<NAME>/*.WAV   套鼓",
        "只支持 16-bit PCM WAV；双声道自动混单",
        "采样池 128 KB；载入套鼓保留约 48 KB 给录音",
        "慢卡：SYS/SD 页看 MB/S 与 STALL",
        "无卡：合成鼓 + NO SD 页，Ent 重试",
    ])

    y2 = section(d, x2, y2, col_w, "05", "PAGES", [
        "PAD   演奏 / 采样入口",
        "SEQ   8 轨 × 16 步，播放头与光标",
        "SMP   波形 / 裁剪（v0.4 只读）",
        "MIX   8 轨电平、M / S、FX 发送",
        "SYS   电量、CPU、堆、SD、欠载",
        "KIT   套鼓列表 + RECORDINGS 导入",
    ]) + 16

    y2 = section(d, x2, y2, col_w, "06", "WEB PREVIEW", [
        "双击 web/index.html，无需服务器",
        "Shift + 1..4 = bank；O（按住）= 16 Levels",
        "REC 2S 模拟录音；IMPORT WAV 载真 WAV",
        "SAVE WAV 导出当前垫",
        "#SEQ #KIT #SD #NOSD #LEVELS #REC 直达",
    ], accent=AQUA) + 16

    y2 = section(d, x2, y2, col_w, "07", "导入 / 导出", [
        "录音自动写 /REC；KIT 页载入 RECORDINGS",
        "电脑：把 WAV 拷到 /KITS/<名字>/ 或 /REC",
        "Web：IMPORT WAV / SAVE WAV",
        "采样率任意，载入时换算；> 4 s 会截断",
        "重命名套鼓目录即出现在 KIT 列表",
    ]) + 16

    y2 = section(d, x2, y2, col_w, "08", "排查", [
        "无声：串口看 [mpc] codec=1；=0 查 ES8311 I2C 0x18",
        "采样无声：0x14 PGA / 0x17 ADC，0x01=0xBA",
        "POOL FULL：换小套鼓或重启清空池",
        "SD 不识别：FAT32、≤32 GB、/KITS /REC",
        "爆音：SYS 看 UNDERRUN / STALL",
    ], accent=RED)

    # signal flow
    fy = max(y1, y2) + 24
    box(d, 48, fy, W - 96, 104, PANEL, EDGE)
    d.text((62, fy + 10), "SIGNAL FLOW", font=f_h, fill=ACC)
    d.text((62, fy + 42),
           "MIC → ES8311 ADC → I2S RX 44.1k → 降采样 22.05k → 采样池 → Pad Voice → Mix → ES8311 DAC → 1 W / 耳机",
           font=f_body, fill=TXT)
    d.text((62, fy + 72),
           "[mpc] boot / codec=1 / sd=SDHC total=… read=…MB/s kits=N      (115200)",
           font=f_small, fill=DIM)

    # page map
    py = fy + 124
    d.text((48, py), "PAGES AT A GLANCE", font=f_h, fill=ACC)
    pages = [
        ("PAD", "演奏 / 采样", ACC),
        ("SEQ", "8 × 16 步进", ACC),
        ("SMP", "波形 / 裁剪", AQUA),
        ("MIX", "电平 / 静音", AQUA),
        ("SYS", "诊断 / SD", YEL),
        ("KIT", "套鼓 / 录音", YEL),
    ]
    pw = (W - 96 - 5 * 12) // 6
    for i, (name, desc, col) in enumerate(pages):
        x = 48 + i * (pw + 12)
        box(d, x, py + 28, pw, 76, PANEL2, EDGE)
        d.text((x + 10, py + 38), name, font=f_h, fill=col)
        d.text((x + 10, py + 68), desc, font=f_small, fill=DIM)

    # specs
    sy = py + 120
    box(d, 48, sy, W - 96, 58, PANEL, EDGE)
    d.text((62, sy + 8), "SPECS", font=f_h, fill=ACC)
    d.text((62, sy + 34),
           "16 声部 · 44.1 kHz 引擎 · 22.05 kHz 采样 · 128 KB 池 · 8×16 步 · 96 PPQN · I2S 全双工 · ES8311 · 1750 mAh",
           font=f_small, fill=DIM)

    # workflows
    wy = sy + 78
    box(d, 48, wy, W - 96, 176, PANEL, EDGE)
    d.text((62, wy + 10), "WORKFLOWS", font=f_h, fill=ACC)
    flows = [
        ("A  采样", ["选垫", "Ent 录 2 s", "自动上垫 + 写 /REC"]),
        ("B  导入", ["WAV 拷到 /KITS 或 /REC", "KIT 选套鼓 / RECORDINGS", "Ent 载入到 16 个垫"]),
        ("C  现场", ["Space 走带", "垫 + Opt 16 Levels", "KIT 换套鼓"]),
    ]
    fw = (W - 96 - 2 * 18) // 3
    for i, (head, steps) in enumerate(flows):
        x = 62 + i * (fw + 18)
        d.text((x, wy + 44), head, font=f_h, fill=AQUA)
        yy = wy + 78
        for k, s in enumerate(steps):
            d.text((x, yy), f"{k+1}. {s}", font=f_small, fill=DIM)
            yy += 24

    d.text((62, wy + 158), "v0.4 完成：麦克风采样 / 上垫 / 存卡 / 导入；下一步：流式播放、MIDI、Song",
           font=f_small, fill=FAINT)

    d.text((W - 250, H - 34), "v0.4.0-dev  ·  MIT", font=f_small, fill=FAINT)

    img.save(OUT)
    print("saved", OUT)


if __name__ == "__main__":
    main()
