# Cardputer_MPC

M5Stack **Cardputer-Adv** 上的采样鼓机 / 步进音序器（"口袋 MPC"）。
Gruvbox Dark、240×135 像素 UI、16 垫、16 声部、8×16 步进、SD 感知。

**状态：v0.3.0-dev** — 固件可编译、Web 预览可运行；**尚未在真机上逐项验收**。
采样录音、流式播放、Song 模式、BLE MIDI 还没做（见下方"实现状态"）。

![web preview](docs/web-preview.png)

---

## 快速开始

### 1. Web 预览（不用刷机）

双击打开 **`web/index.html`**，或拖进浏览器。不需要服务器、不需要构建。

- 点屏幕或按键盘即可演奏；第一次按键/点击后音频才启动（浏览器策略）。
- 左侧是 240×135 的 1:1 画布（4× 放大、最近邻），右侧是可调 BPM / 音量 / SD 状态的模拟面板。
- 地址栏加 `#SEQ`、`#KIT`、`#SD`、`#NOSD`、`#LEVELS` 可直接打开某一屏。

### 2. 固件

```bash
cd firmware
pio run                 # 编译
pio run -t upload       # 烧写（USB-C）
```

或直接烧合并镜像（从 **0x0**）：

```bash
esptool.py --chip esp32s3 -p COMx -b 460800 write_flash 0x0 dist/CardputerMPC-8MB.bin
```

依赖：PlatformIO + `espressif32@6.9.0`；库 `M5Unified 0.3.0`、`M5GFX 0.3.0`、`M5Cardputer 1.1.1`。

---

## 实现状态

### 已经能跑

| 模块 | 状态 |
|---|---|
| 240×135 UI | 6 页（PAD/SEQ/SMP/MIX/SYS/KIT）+ SD/Loading/NoSD 三态 |
| 字体 | Silkscreen 8 px 转 1-bit 点阵（`font8.h`），与设计稿逐像素一致 |
| 音频 | I2S 44.1 kHz、ES8311、16 声部、8 种合成鼓、线性插值采样播放 |
| 音序器 | 96 PPQN、8 轨 × 16 步、播放头、力度、mute |
| 采样 | SD 挂载、套鼓扫描、16-bit PCM WAV（单/双声道、任意采样率换算）载入 128 KB 池 |
| 演奏 | 4×4 垫、bank A–D、16 Levels（按住 Opt）、Note Repeat 预留 |
| 设置 | 音量 / BPM 存 NVS |
| 诊断 | 欠载计数、CPU、堆、SD 容量/读速、STALL |

### 还没做（v0.4+）

- 录音 / 采样 / 裁剪 / 重采样（SMP 页目前只读展示）
- 流式播放（v0.3 全部采样进 RAM；大套鼓会被截断）
- 边载边弹（现在是同步载入 + 进度页）
- 慢卡自动降采样（只测速并在 SYS/SD 页显示，尚未改变载入策略）
- 删除套鼓 / 格式化的长按确认（v0.3 不提供删除和格式化）
- Song 模式、pattern 链、probability/ratchet
- BLE MIDI / Grove UART MIDI
- 真机验收：ES8311 全双工、TCA8418 中断、SD 长时读稳定性

---

## 按键

| 键 | 功能 |
|---|---|
| `1-4 / Q-R / A-F / Z-V` | 触发 pad 1–16（同时选中） |
| `Space` | 播放 / 停止 |
| `Tab` | 下一页 |
| `Ent` | 上下文：SEQ 开关步、KIT 载入、SYS→SD、SD 测速、PAD 录音待命 |
| `Bksp` | 返回 / 清除当前步 |
| `Fn + ; , . /` | 方向键（SEQ 光标、KIT 选择、SMP 选垫） |
| `Fn + 1..4` | bank A–D |
| `Fn + - / =` | BPM −/+ |
| `- / =` | 音量 −/+ |
| `Opt`（按住） | 16 Levels：垫变成 1–16 级力度 |
| `Shift`（Web） | 模拟 Fn（配合 1–4 切 bank） |
| `O`（Web 按住） | 16 Levels |

---

## SD 卡

```
/KITS/<KITNAME>/*.wav     # 每个子目录一个套鼓，最多读 16 个 WAV
/KITS/*.wav               # 也支持平铺
```

- 只支持 **16-bit PCM WAV**；双声道会混成单声道；采样率任意（播放时换算）。
- 128 KB 采样池：超出的文件会被截断/丢弃，KIT 页会标 `STR` / `MISS`。
- 没有卡时自动用内置合成鼓，并进入 `NO SD` 页。
- v0.3 没有流式播放：`STR` 只是标记，实际仍在载入时进 RAM。

---

## 目录

```
Cardputer_MPC/
├─ firmware/            # PlatformIO 工程（ESP32-S3 / Arduino）
│  ├─ src/              # app / audio / samples / ui / font8
│  └─ partitions.csv
├─ web/                 # 免刷机预览（Canvas + WebAudio）
│  ├─ index.html        # 双击打开
│  ├─ app.js            # UI + 音序器 + 合成鼓
│  └─ font8.js          # 与固件同一套点阵
├─ design/              # 设计稿（v0.2）+ 渲染脚本
│  ├─ out/              # 10 张 240×135 设计图
│  ├─ ui-spec.md
│  └─ render_ui.py
├─ tools/gen_font.py    # Silkscreen → 1-bit 字体（固件 + Web）
├─ dist/                # 合并烧写镜像
└─ docs/                # README 截图
```

## 重新生成资源

```bash
python tools/gen_font.py       # 字体 → firmware/src/font8.h + web/font8.js
python design/render_ui.py     # 设计稿 → design/out/*.png
```

## 已知问题

- 固件未经真机验证：ES8311 初始化、I2S 时钟、TCA8418 键盘都按公开资料与先例实现，第一次上机可能需要调寄存器/引脚。
- 合成鼓的噪声是每声部独立 xorshift，但同 age 的声部会相关；听感上可接受，v0.4 换种子。
- SEQ 的 swing 目前只影响 Web 预览，固件内是直拍。
- Web 预览的套鼓是合成的，不代表真实 WAV 音色。

## 许可与致谢

MIT，见 `LICENSE`。

- 配色：[Gruvbox](https://github.com/morhetz/gruvbox)（MIT）
- 字体：[Silkscreen](https://github.com/googlefonts/silkscreen)（SIL OFL 1.1，`design/fonts/OFL.txt`）
- 硬件库：M5Unified / M5GFX / M5Cardputer（MIT）
- ES8311 初始化序列参考 [cardputer-studio](https://github.com/YONGHU-YUAN/cardputer-studio)（MIT）
