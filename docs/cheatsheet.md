# Cardputer_MPC — Cheat Sheet v0.4

> M5Stack Cardputer-Adv · 240×135 · Gruvbox Dark · `v0.4.0-dev`
> 可打印版：[`cheatsheet.png`](cheatsheet.png)

## 60 秒上手

| 步骤 | 动作 |
|---|---|
| 1 | 插一张 FAT32 microSD，建 `/KITS` 与 `/REC` |
| 2 | PAD 页点一个垫（或用方向键选） |
| 3 | 按 `Ent`，对着麦克风，最长 2 s |
| 4 | 自动上垫，并写 `/REC/REC_0001.WAV` |

## 走带 / 页面

| 键 | 功能 |
|---|---|
| `Space` | 播放 / 停止 |
| `Tab` | 下一页：PAD → SEQ → SMP → MIX → SYS → KIT |
| `Ent` | 上下文：PAD 采样 / SEQ 开关步 / KIT 载入 / SYS→SD / SD 测速 |
| `Bksp` | 返回、清步、取消录音 |
| `Fn + ; , . /` | 方向键（SEQ 光标、KIT 选择、SMP 选垫） |
| `Fn + 1..4` | bank A–D |
| `Fn + - / =` | BPM −/+ |
| `- / =` | 音量 −/+ |

## 打击垫

| 键 | 功能 |
|---|---|
| `1 2 3 4` | pad 1–4 |
| `Q W E R` | pad 5–8 |
| `A S D F` | pad 9–12 |
| `Z X C V` | pad 13–16 |
| `Opt`（按住） | 16 Levels：垫变成 1–16 级力度 |
| 垫右下角 | `R` 内存 / `S` 流式 / `!` 缺失 |

## 采样

1. PAD 页选中目标垫。
2. `Ent` 开始录麦克风（最长 2 s，22.05 kHz）。
3. 进度走完：声音上垫 + 自动写 `/REC/REC_0001.WAV`。
4. 录音中 `Bksp` 取消。
5. 没 SD 时只留内存，重启丢失。

**导入**

- `KIT` → `RECORDINGS` → `Ent`，把录音载回 16 个垫。
- 或把 16-bit WAV 拷到 `/KITS/<名字>/` 当套鼓、拷到 `/REC/` 当录音。

## SD / WAV

```
/REC/REC_0001.WAV      录音（自动存）
/KITS/<NAME>/*.WAV     套鼓
```

- 只支持 **16-bit PCM WAV**；双声道自动混单；采样率任意。
- 采样池 128 KB；载入套鼓时保留约 48 KB 给录音。
- 慢卡：SYS / SD 页看 MB/s 与 STALL。
- 无卡：合成鼓 + `NO SD` 页，`Ent` 重试。

## 页面

| 页面 | 内容 |
|---|---|
| PAD | 演奏 / 采样入口 |
| SEQ | 8 轨 × 16 步，播放头与光标 |
| SMP | 波形 / 裁剪（v0.4 只读） |
| MIX | 8 轨电平、M / S、FX 发送 |
| SYS | 电量、CPU、堆、SD、欠载 |
| KIT | 套鼓列表 + `RECORDINGS` 导入 |

## Web 预览

- 双击 `web/index.html`，无需服务器、无需刷机。
- `Shift + 1..4` = bank；`O`（按住）= 16 Levels。
- `REC 2S` 模拟录音；`IMPORT WAV` 载入真实 WAV；`SAVE WAV` 导出当前垫。
- 直达：`#SEQ` `#KIT` `#SD` `#NOSD` `#LEVELS` `#REC`。

## 排查

| 现象 | 检查 |
|---|---|
| 无声 | 串口 `[mpc] codec=1`；=0 查 ES8311 I2C `0x18` |
| 采样无声 | ES8311 `0x14` PGA / `0x17` ADC 音量，`0x01=0xBA` |
| `POOL FULL` | 换小套鼓，或重启清空采样池 |
| SD 不识别 | FAT32、≤32 GB、`/KITS` `/REC` |
| 爆音 | SYS 看 `UNDERRUN` / `STALL`；减少同时流式声部 |

## 信号流

```
MIC → ES8311 ADC → I2S RX 44.1k → 降采样 22.05k → 采样池
    → Pad Voice → Mix → ES8311 DAC → 1 W / 耳机
```

串口 115200：`[mpc] boot / codec=1 / sd=SDHC total=… read=…MB/s kits=N`
