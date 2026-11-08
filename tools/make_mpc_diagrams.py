# -*- coding: utf-8 -*-
"""Generate the diagrams used by docs/可行性评估.md (docs/mpc-*.png).

Run from the Cardputer_MPC/ project root:  python tools/make_mpc_diagrams.py
"""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Rectangle
from matplotlib import font_manager

plt.rcParams["font.family"] = ["Microsoft YaHei"]
plt.rcParams["axes.unicode_minus"] = False

BG = "#f6f7f9"
INK = "#1d2430"
MUTED = "#68707d"
ORANGE = "#e0703c"
BLUE = "#3f6fb5"
TEAL = "#2f8f83"
PURPLE = "#7a5aa6"
GREY = "#9aa2ad"


def rounded(ax, x, y, w, h, fc, ec, lw=1.2, r=1.2, alpha=1.0):
    p = FancyBboxPatch(
        (x, y), w, h,
        boxstyle=f"round,pad=0,rounding_size={r}",
        linewidth=lw, edgecolor=ec, facecolor=fc, alpha=alpha, zorder=2,
    )
    ax.add_patch(p)
    return p


def box(ax, x, y, w, h, title, sub=None, fc="#ffffff", ec=GREY, tcol=INK, scol=MUTED,
        tsize=10.5, ssize=8.2):
    rounded(ax, x, y, w, h, fc, ec)
    ax.text(x + w / 2, y + h - 3.0, title, ha="center", va="top", fontsize=tsize,
            color=tcol, fontweight="bold", zorder=3)
    if sub:
        ax.text(x + w / 2, y + h - 7.2, sub, ha="center", va="top", fontsize=ssize,
                color=scol, linespacing=1.55, zorder=3)


def arrow(ax, p1, p2, color=GREY, style="-|>", lw=1.4, ls="-", rad=0.0):
    a = FancyArrowPatch(p1, p2, arrowstyle=style, mutation_scale=11, linewidth=lw,
                        color=color, linestyle=ls, shrinkA=2, shrinkB=2,
                        connectionstyle=f"arc3,rad={rad}", zorder=4)
    ax.add_patch(a)


# ---------------------------------------------------------------- architecture
def arch():
    fig, ax = plt.subplots(figsize=(13.2, 7.4), dpi=220)
    fig.patch.set_facecolor(BG)
    ax.set_facecolor(BG)
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis("off")

    ax.text(2, 96.5, "Cardputer-Adv MPC 固件架构", fontsize=17, color=INK, fontweight="bold")
    ax.text(2, 92.2, "双核分工：音频链路全程无阻塞，UI / SD 的抖动进不了音频线程",
            fontsize=9.5, color=MUTED)

    # column headers
    for x, label, col in ((2, "Core 0 · UI / IO", BLUE),
                          (37, "共享区 · 无锁", PURPLE),
                          (68, "Core 1 · 音频（prio 20）", ORANGE)):
        ax.text(x, 87.5, label, fontsize=10.5, color=col, fontweight="bold")

    ax.plot([35.5, 35.5], [8, 85], color="#d7dbe2", lw=1, zorder=1)
    ax.plot([66, 66], [8, 85], color="#d7dbe2", lw=1, zorder=1)

    # left column
    box(ax, 2, 66, 30, 17.5, "输入层",
        "TCA8418（IRQ 11）· BMI270\nBLE MIDI · Grove UART MIDI",
        fc="#eef3fb", ec="#b9cbe6")
    box(ax, 2, 46, 30, 16.5, "UI 层",
        "M5GFX Sprite 240×135\n30 fps 局部刷新 · 5 个页面 + 弹窗",
        fc="#eef3fb", ec="#b9cbe6")
    box(ax, 2, 24, 30, 18.5, "存储层",
        "microSD · FAT32\nWAV 解析 / 离线转 22.05 kHz ADPCM\n工程文件 · 套鼓清单",
        fc="#eef3fb", ec="#b9cbe6")
    box(ax, 2, 8, 30, 12.5, "系统", "电池 ADC · 背光 · 休眠 · NVS 设置",
        fc="#eef3fb", ec="#b9cbe6")

    # middle column
    box(ax, 37, 66, 27, 17.5, "事件环 SPSC",
        "pad on/off + 采样时间戳\n容量 64，写满即丢最旧",
        fc="#f3effa", ec="#cfc2e6")
    box(ax, 37, 50, 27, 12.5, "命令队列", "走带 · BPM · 参数 · 音量",
        fc="#f3effa", ec="#cfc2e6")
    box(ax, 37, 30, 27, 16.5, "采样池 192 KB", "one-shot 预载 · 16-bit / IMA ADPCM",
        fc="#f3effa", ec="#cfc2e6")
    box(ax, 37, 12, 27, 14.5, "流式双缓冲", "2 路 × 500 ms 预读 · 32 KB",
        fc="#f3effa", ec="#cfc2e6")

    # right column
    box(ax, 68, 66, 30, 17.5, "时钟 / 音序器",
        "96 PPQN · swing · probability · song\n采样计数器驱动，块内偏移对齐",
        fc="#fdf0e9", ec="#eec4ae")
    box(ax, 68, 46, 30, 17.5, "声部池 ×24",
        "插值重采样 · ADSR · SVF · choke / steal\n音序器按采样点触发，无 malloc",
        fc="#fdf0e9", ec="#eec4ae")
    box(ax, 68, 30, 30, 13.5, "混音 + FX", "delay · bitcrush · limiter",
        fc="#fdf0e9", ec="#eec4ae")
    box(ax, 68, 12, 30, 14.5, "I2S 全双工 DMA → ES8311",
        "1 W 喇叭 / 3.5 mm 耳机 · 44.1 kHz 单声道",
        fc="#fdf0e9", ec="#eec4ae")

    # horizontal data flow
    arrow(ax, (32.2, 74.8), (36.8, 74.8), PURPLE)
    arrow(ax, (64.2, 74.8), (67.8, 74.8), PURPLE)
    arrow(ax, (32.2, 56.2), (36.8, 56.2), PURPLE)
    arrow(ax, (32.2, 38.2), (36.8, 38.2), PURPLE)
    arrow(ax, (64.2, 38.2), (67.8, 40.0), PURPLE)
    arrow(ax, (64.2, 19.2), (67.8, 21.0), PURPLE)

    # right column vertical chain
    arrow(ax, (83, 65.8), (83, 63.8), ORANGE)
    arrow(ax, (83, 45.8), (83, 43.8), ORANGE)
    arrow(ax, (83, 29.8), (83, 26.8), ORANGE)
    ax.text(84.2, 27.6, "44.1 kHz · 128 帧 ≈ 2.9 ms", fontsize=7.4, color=MUTED, va="center")

    # scheduler -> voices (timing) is implied by the vertical chain

    ax.text(2, 4.2,
            "硬约束：音频回调里不做 SD 读、不 malloc、不加锁；SD 卡停顿 100 ms 时，预读缓冲必须兜住。",
            fontsize=8.8, color="#8a3b1e")

    fig.savefig("docs/mpc-arch.png", facecolor=BG, bbox_inches="tight")
    plt.close(fig)


# ------------------------------------------------------------------ memory map
def memory():
    segs = [
        ("框架 / 静态", 90, "#8d99ae"),
        ("显示 Sprite", 65, BLUE),
        ("DMA / 混音", 8, TEAL),
        ("音序器 16 模式", 64, PURPLE),
        ("FX 缓冲", 32, ORANGE),
        ("栈 / 元数据", 22, "#c9a227"),
        ("采样池", 192, "#d94f70"),
        ("余量", 39, "#dfe3e9"),
    ]
    total = 512

    fig, (ax1, ax2) = plt.subplots(
        2, 1, figsize=(12.4, 6.6), dpi=220,
        gridspec_kw={"height_ratios": [1.15, 1.0], "hspace": 0.55},
    )
    for a in (ax1, ax2):
        a.set_facecolor(BG)
    fig.patch.set_facecolor(BG)

    fig.suptitle("512 KB SRAM 怎么分：采样池只剩三分之一多一点",
                 fontsize=15, color=INK, fontweight="bold", x=0.02, ha="left", y=0.98)
    fig.text(0.02, 0.905, "典型配置：关闭 WiFi、BLE 按需开启、16 模式 × 64 步、单主 FX",
             fontsize=9, color=MUTED)

    left = 0
    for name, kb, col in segs:
        ax1.barh(0, kb, left=left, height=0.52, color=col, edgecolor=BG, linewidth=1.5)
        if kb >= 60:
            ax1.text(left + kb / 2, 0, f"{name}\n{kb} KB", ha="center", va="center",
                     fontsize=8.6, color="white" if col != "#dfe3e9" else INK,
                     fontweight="bold", linespacing=1.4)
        left += kb

    # callouts for small segments (centres: DMA 159, FX 243, stack 270)
    small = [(159, "DMA 8", 0.80), (243, "FX 32", 1.00), (270, "栈 22", 1.20)]
    for x, t, y in small:
        ax1.annotate(t, xy=(x, 0.27), xytext=(x, y), ha="center", fontsize=8,
                     color=MUTED, arrowprops=dict(arrowstyle="-", color="#b6bcc6", lw=0.8))

    ax1.set_xlim(0, total)
    ax1.set_ylim(-0.55, 1.42)
    ax1.set_yticks([])
    ax1.set_xticks([0, 128, 256, 384, 512])
    ax1.tick_params(colors=MUTED, labelsize=8.5)
    for s in ("top", "right", "left"):
        ax1.spines[s].set_visible(False)
    ax1.spines["bottom"].set_color("#c9ced6")
    ax1.set_xlabel("KB", color=MUTED, fontsize=8.5, loc="right")
    ax1.set_title("", pad=0)

    # sample seconds
    formats = [
        ("44.1 kHz / 16-bit", 2.2, ORANGE),
        ("32 kHz / 16-bit", 3.1, "#e8a33d"),
        ("22.05 kHz / 16-bit", 4.5, TEAL),
        ("22.05 kHz / 4-bit ADPCM", 17.8, BLUE),
    ]
    names = [f[0] for f in formats][::-1]
    vals = [f[1] for f in formats][::-1]
    cols = [f[2] for f in formats][::-1]
    bars = ax2.barh(names, vals, height=0.55, color=cols)
    for b, v in zip(bars, vals):
        ax2.text(v + 0.25, b.get_y() + b.get_height() / 2, f"{v:.1f} s", va="center",
                 fontsize=9.5, color=INK, fontweight="bold")
    ax2.set_xlim(0, 21)
    ax2.set_xticks([0, 5, 10, 15, 20])
    ax2.tick_params(colors=MUTED, labelsize=8.5)
    ax2.set_xlabel("可容纳素材时长（秒）", color=MUTED, fontsize=8.5, loc="right")
    for s in ("top", "right", "left"):
        ax2.spines[s].set_visible(False)
    ax2.spines["bottom"].set_color("#c9ced6")
    ax2.grid(axis="x", color="#e2e5ea", lw=0.7, zorder=0)
    ax2.set_axisbelow(True)
    ax2.set_title("同样 192 KB，换个存储格式能多装 8 倍", fontsize=10.5, color=INK,
                  loc="left", pad=10)

    fig.savefig("docs/mpc-memory.png", facecolor=BG, bbox_inches="tight")
    plt.close(fig)


# --------------------------------------------------------------------- keymap
def keymap():
    rows = [
        ["`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Bksp"],
        ["Tab", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\"],
        ["Fn", "Aa", "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "Ent"],
        ["Ctrl", "Opt", "Alt", "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "Space"],
    ]
    pads = {(0, 1): 1, (0, 2): 2, (0, 3): 3, (0, 4): 4,
            (1, 1): 5, (1, 2): 6, (1, 3): 7, (1, 4): 8,
            (2, 2): 9, (2, 3): 10, (2, 4): 11, (2, 5): 12,
            (3, 3): 13, (3, 4): 14, (3, 5): 15, (3, 6): 16}
    transport = {(3, 13): "播放/停止", (2, 13): "录音", (1, 0): "切页", (0, 13): "撤销"}
    mods = {(2, 0): "组合", (2, 1): "重复", (3, 0): "擦除", (3, 1): "16 Lv", (3, 2): "静音"}

    fig, ax = plt.subplots(figsize=(13.4, 5.2), dpi=220)
    fig.patch.set_facecolor(BG)
    ax.set_facecolor(BG)
    ax.axis("off")
    ax.set_xlim(0, 14.6)
    ax.set_ylim(-0.35, 5.4)

    ax.text(0, 5.15, "键盘即打击垫：左手 4×4，右手走带", fontsize=16, color=INK,
            fontweight="bold")
    ax.text(0, 4.62, "橙色 = pad 1–16（bank A）；蓝色 = 走带 / 模式；绿色 = 组合键；Fn+1–4 切 bank A–D",
            fontsize=9, color=MUTED)

    w, h, gx, gy = 0.92, 0.72, 0.06, 0.10
    for r, row in enumerate(rows):
        y = 3.55 - r * (h + gy)
        for c, label in enumerate(row):
            x = 0.25 + c * (w + gx)
            sub = None
            if (r, c) in pads:
                fc, ec, tc, fs = "#f6d9c8", ORANGE, "#8a3b1e", 10.5
            elif (r, c) in transport:
                fc, ec, tc, fs = "#dbe6f7", BLUE, "#22406e", 9.6
                sub = transport[(r, c)]
            elif (r, c) in mods:
                fc, ec, tc, fs = "#d9ece8", TEAL, "#1d5a52", 9.4
                sub = mods[(r, c)]
            else:
                fc, ec, tc, fs = "#ffffff", "#d3d8df", MUTED, 9.5
            rounded(ax, x, y, w, h, fc, ec, lw=1.2, r=0.14)
            ax.text(x + w / 2, y + h / 2 + (0.09 if sub else -0.02), label,
                    ha="center", va="center", fontsize=fs, color=tc,
                    fontweight="bold" if (r, c) in pads else "normal")
            if sub:
                ax.text(x + w / 2, y + 0.16, sub, ha="center", va="center",
                        fontsize=6.4, color=tc)
            if (r, c) in pads:
                ax.text(x + w - 0.09, y + h - 0.06, str(pads[(r, c)]), ha="right", va="top",
                        fontsize=6.2, color=ORANGE)

    # legend
    lx, ly = 0.25, 0.30
    for label, col, fcol in (("打击垫", ORANGE, "#f6d9c8"),
                             ("走带 / 模式", BLUE, "#dbe6f7"),
                             ("组合键", TEAL, "#d9ece8")):
        rounded(ax, lx, ly - 0.14, 0.26, 0.26, fcol, col, lw=1.0, r=0.05)
        ax.text(lx + 0.36, ly, label, fontsize=9, color=INK, va="center")
        lx += 1.55

    ax.text(5.35, 0.42,
            "没有力度：默认全速击发，「Opt + pad」映射 16 Levels，「Fn+0」切换 Full Level；外接 BLE MIDI 才有无级力度。",
            fontsize=8.6, color=MUTED, va="center")
    ax.text(5.35, 0.02,
            "TCA8418 支持 10 键以上同时识别，四指和弦可用；但仍然没有力度。",
            fontsize=8.6, color=MUTED, va="center")

    fig.savefig("docs/mpc-keymap.png", facecolor=BG, bbox_inches="tight")
    plt.close(fig)


# ------------------------------------------------------------------ ui mockup
def ui_mockup():
    SCR_W, SCR_H = 240, 135
    fig, ax = plt.subplots(figsize=(11.2, 3.55), dpi=220)
    fig.patch.set_facecolor(BG)
    ax.set_facecolor(BG)
    ax.axis("off")
    ax.set_xlim(-6, 2 * SCR_W + 18)
    ax.set_ylim(-26, SCR_H + 20)

    ax.text(-4, SCR_H + 15, "240×135 上的两屏：PAD 与 SEQ", fontsize=13, color=INK,
            fontweight="bold")
    ax.text(-4, SCR_H + 6, "实机 1.14 英寸，字号 6–8 px；每屏只回答一个问题：现在按哪个垫 / 哪一步是开的",
            fontsize=7.6, color=MUTED)

    def screen(ox):
        ax.add_patch(Rectangle((ox, 0), SCR_W, SCR_H, facecolor="#11161d",
                               edgecolor="#2a323d", lw=1.0, zorder=1))
        ax.add_patch(Rectangle((ox, SCR_H - 12), SCR_W, 12, facecolor="#182029",
                               edgecolor="none", zorder=2))

    # ---------------- PAD screen
    ox = 0
    screen(ox)
    ax.text(ox + 3, SCR_H - 8.2, "PAD  A", fontsize=5.6, color="#f0a06a",
            fontweight="bold", zorder=3)
    ax.text(ox + 46, SCR_H - 8.2, "92.0", fontsize=5.6, color="#e8edf4", zorder=3)
    ax.text(ox + 66, SCR_H - 8.2, "SW 54%", fontsize=5.0, color="#8d98a6", zorder=3)
    ax.add_patch(Rectangle((ox + 206, SCR_H - 8.6), 18, 4.4, facecolor="#2a323d",
                           edgecolor="none", zorder=3))
    ax.add_patch(Rectangle((ox + 206, SCR_H - 8.6), 13, 4.4, facecolor="#6fbf9a",
                           edgecolor="none", zorder=3))
    ax.text(ox + 227, SCR_H - 8.2, "78", fontsize=4.6, color="#8d98a6", zorder=3)

    names = ["KICK", "SNARE", "CLAP", "HAT_C", "HAT_O", "TOM_L", "TOM_H", "RIM",
             "SHAK", "COW", "PERC1", "PERC2", "FX1", "FX2", "VOX", "EMPTY"]
    active = {0, 5}
    for i in range(16):
        r, c = divmod(i, 4)
        x = ox + 4 + c * 30
        y = SCR_H - 18 - (r + 1) * 26 + 2
        on = i in active
        ax.add_patch(FancyBboxPatch((x, y), 27, 23,
                                    boxstyle="round,pad=0,rounding_size=2",
                                    facecolor="#3a2318" if on else "#1a222c",
                                    edgecolor=ORANGE if on else "#2d3743", lw=1.0, zorder=3))
        ax.text(x + 2, y + 19, str(i + 1), fontsize=4.8,
                color=ORANGE if on else "#63707f", zorder=4)
        ax.text(x + 2, y + 11, names[i], fontsize=5.0,
                color="#ffd9c2" if on else "#aab4c0", zorder=4)
        if on:
            ax.add_patch(Rectangle((x + 2, y + 3), 8, 2.2, facecolor=ORANGE,
                                   edgecolor="none", zorder=4, alpha=0.9))

    px = ox + 128
    ax.text(px, SCR_H - 22, "KICK_808", fontsize=6.4, color="#e8edf4",
            fontweight="bold", zorder=3)
    ax.text(px, SCR_H - 30, "A01 · 0.42 s · 22.05k", fontsize=4.6, color="#8d98a6", zorder=3)
    params = [("VOL", 0.82), ("TUNE", 0.50), ("CUT", 0.74), ("DEC", 0.38)]
    for k, (label, v) in enumerate(params):
        yy = SCR_H - 43 - k * 14
        ax.text(px, yy, label, fontsize=4.6, color="#8d98a6", zorder=3)
        ax.add_patch(Rectangle((px + 20, yy - 1.2), 52, 3.6, facecolor="#232c37",
                               edgecolor="none", zorder=3))
        ax.add_patch(Rectangle((px + 20, yy - 1.2), 52 * v, 3.6, facecolor="#5b8fd6",
                               edgecolor="none", zorder=3))
    ax.text(px, SCR_H - 101, "MODE  ONE-SHOT", fontsize=4.6, color="#8d98a6", zorder=3)
    ax.text(px, SCR_H - 109, "CHOKE  HAT", fontsize=4.6, color="#8d98a6", zorder=3)
    ax.text(px, SCR_H - 119, "Fn+1..4 BANK   Opt 16LV", fontsize=4.2, color="#4f5b69", zorder=3)

    # ---------------- SEQ screen
    ox = SCR_W + 18
    screen(ox)
    ax.text(ox + 3, SCR_H - 8.2, "SEQ  P1", fontsize=5.6, color="#f0a06a",
            fontweight="bold", zorder=3)
    ax.text(ox + 46, SCR_H - 8.2, "92.0", fontsize=5.6, color="#e8edf4", zorder=3)
    ax.text(ox + 66, SCR_H - 8.2, "1/16  BAR 1/4", fontsize=4.8, color="#8d98a6", zorder=3)
    ax.text(ox + 196, SCR_H - 8.2, "REC ●", fontsize=4.8, color="#e05a5a", zorder=3)

    tracks = ["01 KICK", "02 SNR", "03 CLAP", "04 HAT_C", "05 HAT_O", "06 TOM", "07 RIM", "08 FX"]
    pattern = [
        [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0],
        [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0],
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0],
        [1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0],
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0],
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0],
        [0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0],
    ]
    gx, gy, cw, ch = ox + 26, SCR_H - 20, 12.6, 12.4
    # playhead column first, so the step blocks draw on top
    ax.add_patch(Rectangle((gx + 5 * cw, gy - 8 * ch), cw - 1.1, 8 * ch,
                           facecolor="#ffffff", alpha=0.07, edgecolor="#cfd8e3",
                           lw=0.7, zorder=2))
    for r in range(8):
        y = gy - (r + 1) * ch
        ax.text(ox + 3, y + 4.2, tracks[r], fontsize=4.4, color="#8d98a6", zorder=3)
        for c in range(16):
            on = pattern[r][c]
            beat = c % 4 == 0
            ax.add_patch(Rectangle((gx + c * cw, y), cw - 1.1, ch - 1.4,
                                   facecolor="#3a2318" if on else ("#1c2530" if beat else "#161d26"),
                                   edgecolor="#2d3743", lw=0.5, zorder=3))
            if on:
                ax.add_patch(Rectangle((gx + c * cw + 2.2, y + 2.6), cw - 5.4, ch - 6.4,
                                       facecolor=ORANGE, edgecolor="none", zorder=4))

    ax.text(ox + 3, 4, "Fn+,/. 移调   Ent 切换   Space 播放   Ctrl+垫 清除", fontsize=4.3,
            color="#4f5b69", zorder=3)

    fig.savefig("docs/mpc-ui.png", facecolor=BG, bbox_inches="tight")
    plt.close(fig)


if __name__ == "__main__":
    arch()
    memory()
    keymap()
    ui_mockup()
    print("ok")
