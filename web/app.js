/* Cardputer MPC — web preview
 * Mirrors the firmware UI (Gruvbox Dark, 240x135, 8px Silkscreen) and
 * synthesises a small drum kit with WebAudio so the pads make sound.
 */
window.addEventListener('error', e => { document.title = 'ERR: ' + e.message; });
(() => {
  'use strict';

  const W = 240, H = 135;
  const cvs = document.getElementById('lcd');
  const ctx = cvs.getContext('2d');
  ctx.imageSmoothingEnabled = false;

  // ------------------------------------------------------------ palette
  const C = {
    bg: '#1d2021', bar: '#282828', panel: '#282828', pad: '#32302f',
    edge: '#504945', line: '#3c3836', txt: '#ebdbb2', dim: '#a89984',
    faint: '#7c6f64', acc: '#fe8019', accDim: '#4a2a17', amb: '#fabd2f',
    aqua: '#8ec07c', blue: '#83a598', red: '#fb4934', green: '#b8bb26',
    play: '#3f2a1b', sel: '#bdae93', inv: '#fbf1c7'
  };

  // ------------------------------------------------------------ state
  const app = {
    page: 'PAD', sd: 'ready', playing: false, recArm: false,
    bank: 0, selected: 0, cursorTrack: 0, cursorStep: 0,
    bpm: 92, swing: 54, levelsMode: false, levelValue: 64,
    volume: 78, battery: 78, cpu: 34, underruns: 0, stalls: 0, bufPct: 62,
    sdTotal: 29.7, sdFree: 12.4, sdRead: 1.8,
    kitSel: 0, loadingPct: 0, loadingName: '', toast: '', toastUntil: 0,
    playhead: 0, recording: false, recPct: 0
  };

  const KITS = [
    { name: '808 CORE', samples: 16, bytes: 2.1, mode: 'STR' },
    { name: '909', samples: 16, bytes: 1.8, mode: 'STR' },
    { name: 'POCKET 4', samples: 4, bytes: 0.08, mode: 'RAM' },
    { name: 'ACOUSTIC', samples: 16, bytes: 3.4, mode: 'STR' },
    { name: 'USER 01', samples: 12, bytes: 0.9, mode: 'MISS' },
    { name: 'EMPTY', samples: 0, bytes: 0, mode: 'MISS' }
  ];

  const NAMES = ['KICK', 'SNR', 'CLAP', 'HATC', 'HATO', 'TOML', 'TOMH', 'RIM',
                 'SHAK', 'COW', 'PERC', 'PER2', 'FX1', 'FX2', 'VOX', '--'];
  const FULL = ['KICK_808', 'SNR_909', 'CLAP_808', 'HAT_CLOSED', 'HAT_OPEN', 'TOM_LOW',
                'TOM_HIGH', 'RIM_808', 'SHAKER', 'COWBELL', 'PERC_1', 'PERC_2',
                'FX_1', 'FX_2', 'VOX', 'EMPTY'];

  const pads = NAMES.map((n, i) => ({
    name: n, full: FULL[i], kind: i === 15 ? 'empty' : (i < 8 ? 'sample' : 'synth'),
    cache: i === 15 ? null : (i < 8 ? 'R' : 'S'), synth: i % 8, buffer: null
  }));

  const padLevel = new Float32Array(16);

  const pattern = Array.from({ length: 8 }, () => Array.from({ length: 16 }, () => ({ on: 0, vel: 100 })));
  const VELS = [
    [127, 0, 0, 0, 96, 0, 0, 0, 118, 0, 0, 0, 110, 0, 62, 0],
    [0, 0, 0, 0, 112, 0, 0, 0, 0, 0, 0, 0, 108, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 90, 0, 0, 0],
    [88, 0, 54, 0, 92, 0, 48, 0, 86, 0, 52, 0, 96, 0, 60, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 78, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 84, 0, 0],
    [0, 0, 0, 96, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 70, 0, 0, 0, 0, 0, 0]
  ];
  for (let r = 0; r < 8; r++) for (let c = 0; c < 16; c++) {
    pattern[r][c].on = VELS[r][c] ? 1 : 0;
    pattern[r][c].vel = VELS[r][c] || 100;
  }

  // ------------------------------------------------------------ text
  const F = { reg: FONT8, bold: FONT8B };

  function text(x, y, s, col, bold = false) {
    x = Math.floor(x); y = Math.floor(y);
    const tbl = bold ? F.bold : F.reg;
    ctx.fillStyle = col;
    let cx = x;
    for (const ch0 of String(s)) {
      let ch = ch0;
      if (ch >= 'a' && ch <= 'z') ch = ch.toUpperCase();
      let code = ch.charCodeAt(0);
      if (code < 32 || code > 95) code = 63;
      const g = tbl[code - 32];
      for (let row = 0; row < 8; row++) {
        const bits = g[row];
        for (let b = 0; b < 6; b++) {
          if (bits & (1 << b)) ctx.fillRect(cx + b, y + row, 1, 1);
        }
      }
      cx += 6;
      if (cx > W) break;
    }
  }

  function fmt(f, ...a) {
    let i = 0;
    return f.replace(/%(-?\d+)?(?:\.(\d+))?([dsf])/g, (m, pad, prec, type) => {
      const v = a[i++];
      if (type === 'd') {
        let s = String(Math.trunc(v));
        if (pad) s = s.padStart(parseInt(pad, 10), ' ');
        return s;
      }
      if (type === 'f') return Number(v).toFixed(prec ? parseInt(prec, 10) : 1);
      return String(v);
    });
  }
  const textf = (x, y, col, bold, f, ...a) => text(x, y, fmt(f, ...a), col, bold);
  const tw = s => String(s).length * 6;

  // ------------------------------------------------------------ shapes
  const rect = (x, y, w, h, c) => { ctx.fillStyle = c; ctx.fillRect(x, y, w, h); };
  const frame = (x, y, w, h, c) => {
    rect(x, y, w, 1, c); rect(x, y + h - 1, w, 1, c);
    rect(x, y, 1, h, c); rect(x + w - 1, y, 1, h, c);
  };
  const hline = (x, y, w, c) => rect(x, y, w, 1, c);
  const vline = (x, y, h, c) => rect(x, y, 1, h, c);

  function bar(x, y, w, h, ratio, fg) {
    rect(x, y, w, h, C.edge);
    const fw = Math.max(0, Math.min(w - 2, Math.round((w - 2) * Math.max(0, Math.min(1, ratio)))));
    if (fw > 0) rect(x + 1, y + 1, fw, h - 2, fg);
  }

  function chip(x, y, w, h, s, active = false, danger = false) {
    if (active) {
      rect(x, y, w, h, C.acc);
      text(x + ((w - tw(s)) >> 1), y + 3, s, C.bg, true);
    } else {
      frame(x, y, w, h, danger ? C.red : C.edge);
      text(x + ((w - tw(s)) >> 1), y + 3, s, danger ? C.red : C.dim);
    }
  }

  function battery(x, y, pct) {
    frame(x, y, 12, 6, C.dim);
    rect(x + 12, y + 2, 2, 2, C.dim);
    const n = pct > 70 ? 8 : pct > 35 ? 5 : 2;
    rect(x + 1, y + 1, n, 4, pct > 35 ? C.green : C.red);
  }

  const sdColor = () => (app.page === 'NOSD' || app.sd === 'none') ? C.red
                   : app.sd === 'loading' ? C.amb : C.green;
  const sdTag = (x, y) => text(x, y, 'SD', sdColor());

  // ------------------------------------------------------------ top / bottom
  const TABS = ['PAD', 'SEQ', 'SMP', 'MIX', 'SYS', 'KIT'];
  const TRACKS = ['KIK', 'SNR', 'CLP', 'HAT', 'OPN', 'TOM', 'RIM', 'FX'];
  function tabIndex() {
    if (app.page === 'SD') return 4;
    const i = TABS.indexOf(app.page);
    return i < 0 ? 0 : i;
  }

  function topbar() {
    rect(0, 0, W, 12, C.bar);
    hline(0, 11, W, C.line);
    const a = tabIndex();
    TABS.forEach((t, i) => {
      const x = 3 + i * 24;
      if (i === a) {
        text(x, 2, t, C.acc, true);
        hline(x, 10, tw(t), C.acc);
      } else {
        text(x, 2, t, C.faint);
      }
    });
    textf(150, 2, C.txt, true, '%d.0', app.bpm);
    text(178, 2, 'BPM', C.faint);
    sdTag(204, 2);
    battery(224, 3, app.battery);
  }

  function bottombar(s) {
    rect(0, 124, W, 11, C.bar);
    hline(0, 124, W, C.line);
    text(3, 126, s, C.faint);
  }

  function toast() {
    if (!app.toast || performance.now() > app.toastUntil) return;
    const w = Math.min(W - 8, tw(app.toast) + 8);
    rect(4, 108, w, 13, C.panel);
    frame(4, 108, w, 13, C.acc);
    text(8, 111, app.toast, C.txt);
  }

  // ------------------------------------------------------------ waveforms
  const waveCache = new Map();
  function waveFor(idx) {
    if (waveCache.has(idx)) return waveCache.get(idx);
    const type = (idx % 8);
    const n = 256, a = new Float32Array(n);
    let ph = 0, lp = 0, seed = 1234 + idx * 77;
    const rnd = () => {
      seed = (seed * 1664525 + 1013904223) >>> 0;
      return (seed / 4294967296) * 2 - 1;
    };
    for (let i = 0; i < n; i++) {
      const t = i / n;
      let s = 0;
      switch (type) {
        case 0: { const f = 44 + 120 * Math.exp(-t * 30); ph += f / 800; s = Math.sin(ph * 6.283) * Math.exp(-t * 7) + (t < 0.02 ? 0.4 : 0); break; }
        case 1: s = (rnd() * 0.7 + Math.sin(t * 120) * 0.35) * Math.exp(-t * 9); break;
        case 2: s = rnd() * Math.exp(-t * 11) * (0.6 + 0.4 * Math.sin(t * 60)); break;
        case 3: lp += (rnd() - lp) * 0.35; s = (rnd() - lp) * Math.exp(-t * 40); break;
        case 4: lp += (rnd() - lp) * 0.35; s = (rnd() - lp) * Math.exp(-t * 7); break;
        case 5: { const f = 90 + 90 * Math.exp(-t * 15); ph += f / 700; s = Math.sin(ph * 6.283) * Math.exp(-t * 7); break; }
        case 6: s = (rnd() * 0.5 + Math.sin(t * 300) * 0.5) * Math.exp(-t * 30); break;
        default: lp += (rnd() - lp) * 0.5; s = (rnd() - lp) * Math.exp(-t * 14); break;
      }
      a[i] = s;
    }
    waveCache.set(idx, a);
    return a;
  }

  function drawWave(x, y, w, h, idx, col) {
    const a = waveFor(idx);
    const cy = y + h / 2;
    ctx.fillStyle = col;
    for (let i = 0; i < w; i++) {
      const s = a[Math.floor(i * a.length / w)];
      const hh = Math.abs(s) * (h / 2 - 2);
      if (hh >= 0.5) ctx.fillRect(x + i, Math.round(cy - hh), 1, Math.max(1, Math.round(hh * 2)));
    }
  }

  // ------------------------------------------------------------ pages
  function pageRec() {
    const x = 18, y = 26, w = 204, h = 80;
    rect(x, y, w, h, C.panel);
    frame(x, y, w, h, C.red);

    text(x + 8, y + 8, 'SAMPLING', C.red, true);
    textf(x + 132, y + 8, C.dim, false, 'PAD %02d', app.selected + 1);

    bar(x + 8, y + 24, w - 16, 8, app.recPct / 100, C.red);
    textf(x + 8, y + 38, C.txt, true, '%d%', app.recPct);
    textf(x + 44, y + 38, C.faint, false, 'SIMULATED MIC');
    text(x + 8, y + 54, 'REC -> PAD  AUTO SAVE SD', C.faint);
    text(x + 8, y + 66, 'BKS CANCEL', C.dim);
    bottombar('BKS CANCEL');
  }

  function pagePad() {
    if (app.levelsMode) return pageLevels();

    for (let i = 0; i < 16; i++) {
      const x = 4 + (i % 4) * 30, y = 15 + Math.floor(i / 4) * 27;
      const p = pads[i];
      const play = padLevel[i] > 6;
      rect(x, y, 28, 25, play ? C.play : C.pad);
      frame(x, y, 28, 25, play ? C.acc : C.edge);
      if (i === app.selected) frame(x - 1, y - 1, 30, 27, C.sel);

      if (p.kind === 'empty') text(x + 2, y + 3, '--', C.faint);
      else text(x + 2, y + 3, p.name, play ? C.inv : C.txt, play);
      textf(x + 2, y + 14, play ? C.acc : C.faint, false, '%d', i + 1);
      if (p.cache === 'R') text(x + 21, y + 13, 'R', C.dim, true);
      else if (p.cache === 'S') text(x + 21, y + 13, 'S', C.aqua, true);
      else if (p.cache === '!') text(x + 21, y + 13, '!', C.red, true);

      if (padLevel[i] > 4) {
        rect(x + 3, y + 20, 22, 2, C.accDim);
        rect(x + 3, y + 20, Math.round(22 * padLevel[i] / 127), 2, C.acc);
      }
    }

    const px = 127, p = pads[app.selected];
    text(px, 15, p.kind === 'empty' ? 'EMPTY' : p.full, C.txt, true);
    if (p.kind === 'sample') textf(px, 26, C.faint, false, '%.2fS 22K R', 0.42);
    else if (p.kind === 'synth') text(px, 26, 'SYNTH', C.faint);
    else text(px, 26, 'NO SAMPLE', C.faint);
    hline(px, 36, 109, C.line);

    rect(px, 39, 109, 16, '#181a1b');
    frame(px, 39, 109, 16, C.line);
    drawWave(px + 1, 41, 107, 12, app.selected, '#cd6c3c');

    ['VOL', 'TUNE', 'CUT', 'DEC'].forEach((n, i) => {
      const y = 60 + i * 14;
      text(px, y, n, C.dim);
      bar(px + 32, y + 2, 76, 5, [0.82, 0.5, 0.74, 0.38][i], i ? C.blue : C.acc);
    });

    chip(px, 112, 40, 11, '1-SHOT', true);
    chip(px + 44, 112, 34, 11, 'CHOKE');
    chip(px + 82, 112, 24, 11, '16LV');
    bottombar('FN+1-4 BANK  OPT 16LV  SPC PLAY');
  }

  function pageLevels() {
    for (let i = 0; i < 16; i++) {
      const x = 4 + (i % 4) * 30, y = 15 + Math.floor(i / 4) * 27;
      const k = i / 15;
      const vel = 8 + Math.round(i * 119 / 15);
      const r = Math.round(46 + 40 * k), g = Math.round(34 + 16 * k), b = Math.round(24 + 8 * k);
      rect(x, y, 28, 25, `rgb(${r},${g},${b})`);
      frame(x, y, 28, 25, i === app.selected ? C.acc : C.edge);
      textf(x + 2, y + 2, C.faint, false, '%d', i + 1);
      textf(x + 5, y + 10, k > 0.35 ? C.inv : C.dim, true, '%d', vel);
    }
    frame(4 + (app.selected % 4) * 30 - 1, 15 + Math.floor(app.selected / 4) * 27 - 1, 30, 27, C.sel);

    const px = 127, p = pads[app.selected];
    text(px, 15, p.kind === 'empty' ? 'EMPTY' : p.full, C.txt, true);
    text(px, 26, '16 LEVELS', C.acc);
    hline(px, 36, 109, C.line);
    text(px, 44, 'VELOCITY', C.dim);
    textf(px, 54, C.acc, true, '%d', app.levelValue);
    for (let i = 0; i < 16; i++) {
      rect(px + i * 6, 66, 5, 7, i < (app.levelValue * 16 / 127) ? C.acc : C.edge);
    }
    text(px, 82, 'RANGE 8-127', C.faint);
    text(px, 94, 'OPT HELD', C.acc);
    textf(px, 106, C.faint, false, 'PAD %02d', app.selected + 1);
    bottombar('TAP PAD SET VEL  RELEASE OPT EXIT');
  }

  function pageSeq() {
    text(3, 14, 'PAT 01', C.acc, true);
    text(52, 14, 'BAR 1/4', C.faint);
    textf(118, 14, C.faint, false, 'SW %d', app.swing);
    text(214, 14, 'REC', C.red);
    rect(232, 16, 4, 4, C.red);

    const x0 = 25, y0 = 24;
    for (let tr = 0; tr < 8; tr++) {
      const y = y0 + tr * 12;
      text(3, y + 1, TRACKS[tr], C.dim);
      for (let c = 0; c < 16; c++) {
        const x = x0 + c * 13;
        const beat = c % 4 === 0;
        const base = (app.playing && c === app.playhead) ? C.edge : (beat ? '#2c2928' : '#242221');
        rect(x, y, 12, 9, base);
        const s = pattern[tr][c];
        if (s.on) {
          const k = s.vel / 127;
          rect(x + 1, y + 1, 10, 7, `rgb(${Math.round(120 + 134 * k)},${Math.round(58 + 70 * k)},${Math.round(30 - 5 * k)})`);
        }
      }
    }

    if (app.playing) {
      const x = x0 + app.playhead * 13;
      ctx.fillStyle = C.txt;
      ctx.beginPath();
      ctx.moveTo(x + 2, 21); ctx.lineTo(x + 9, 21); ctx.lineTo(x + 5, 23);
      ctx.closePath(); ctx.fill();
    }
    frame(x0 + app.cursorStep * 13, y0 + app.cursorTrack * 12, 12, 9, C.txt);
    bottombar('FN ARROWS MOVE  ENT TOGGLE  SPC PLAY');
  }

  function pageSmp() {
    const p = pads[app.selected];
    text(3, 14, p.kind === 'empty' ? 'EMPTY' : p.full, C.txt, true);
    text(58, 14, p.kind === 'sample' ? 'RAM' : 'SYNTH', C.dim);
    textf(196, 14, C.dim, false, '%.2fS', p.kind === 'sample' ? 0.42 : 0.31);

    const x = 3, y = 24, w = 233, h = 58;
    rect(x, y, w, h, '#181a1b');
    frame(x, y, w, h, C.line);
    hline(x, y + h / 2, w, C.line);
    drawWave(x + 1, y + 2, w - 2, h - 4, app.selected, '#d5764c');

    const sx = 10, ex = 212;
    vline(x + sx, y, h, C.aqua);
    vline(x + ex, y, h, C.aqua);
    ctx.fillStyle = C.aqua;
    ctx.beginPath(); ctx.moveTo(x + sx - 3, y); ctx.lineTo(x + sx + 3, y); ctx.lineTo(x + sx, y + 4); ctx.fill();
    ctx.beginPath(); ctx.moveTo(x + ex - 3, y); ctx.lineTo(x + ex + 3, y); ctx.lineTo(x + ex, y + 4); ctx.fill();

    for (let k = 0; k < 4; k++) {
      const tx = x + 1 + Math.round(k * 0.1 / 0.42 * (w - 2));
      vline(tx, y + h - 4, 4, '#605952');
      textf(tx + 2, y + h - 9, C.faint, false, '0.%d', k);
    }

    text(3, 86, 'ST 0.000', C.dim);
    text(62, 86, 'END 0.420', C.dim);
    text(126, 86, 'LEN 0.420', C.dim);
    text(196, 86, '22.05K', C.faint);

    chip(3, 100, 32, 12, 'NORM');
    chip(39, 100, 26, 12, 'REV');
    chip(69, 100, 32, 12, 'TRIM');
    chip(105, 100, 50, 12, 'SLICE 8', true);
    chip(203, 100, 32, 12, 'ZOOM');
    bottombar('FN+,- TRIM  ENT PREVIEW  BKS BACK');
  }

  function pageMix() {
    text(3, 14, 'TRK', C.faint);
    text(52, 14, 'LEVEL', C.faint);
    text(168, 14, 'FX SEND', C.faint);
    const base = [0.82, 0.74, 0.48, 0.61, 0.44, 0.52, 0.28, 0.36];
    for (let i = 0; i < 8; i++) {
      const y = 23 + i * 12;
      text(3, y + 1, TRACKS[i], C.txt);
      frame(25, y, 9, 9, C.edge);
      text(27, y + 1, 'M', C.faint);
      frame(37, y, 9, 9, C.edge);
      text(39, y + 1, 'S', C.faint);
      const lv = app.playing ? padLevel[i] / 127 : base[i];
      bar(50, y + 2, 104, 6, lv, C.acc);
      bar(168, y + 2, 64, 6, base[7 - i] * 0.6, C.aqua);
    }
    bottombar('M/S MUTE SOLO  FN ARROWS  ENT SOLO');
  }

  function pageSys() {
    textf(4, 16, C.green, true, 'BATT %d%%', app.battery);
    bar(4, 26, 100, 4, app.battery / 100, C.green);

    text(4, 36, 'CPU', C.dim);
    textf(34, 36, C.txt, true, '%d%%', app.cpu);
    bar(4, 45, 110, 4, app.cpu / 100, C.blue);

    text(4, 52, 'MEM', C.dim);
    textf(34, 52, C.txt, true, '%dK', 118);
    bar(4, 61, 110, 4, 0.62, C.amb);

    text(4, 70, 'VOICES 16', C.dim);
    text(4, 82, 'PADS 16/16', C.dim);

    if (app.sd === 'ready') {
      text(124, 16, 'SD READY', C.green, true);
      textf(124, 28, C.txt, true, '%.1fG FREE', app.sdFree);
      textf(124, 38, C.faint, false, '%.1f MB/S', app.sdRead);
      textf(124, 50, C.dim, false, 'STALL %d', app.stalls);
    } else {
      text(124, 16, 'SD NONE', C.red, true);
    }
    text(124, 62, 'MIDI BLE OFF', C.faint);
    text(124, 74, 'FW 0.3.0', C.faint);

    rect(4, 96, 232, 24, '#181a1b');
    frame(4, 96, 232, 24, C.line);
    text(8, 105, 'UNDERRUN', C.dim);
    textf(62, 105, C.green, true, '%d', app.underruns);
    textf(80, 105, C.faint, false, 'LAT 2.9MS');
    textf(160, 105, C.faint, false, 'POOL 62%');
    bottombar('ENT SD  BKS BACK  HOLD G0 RESET');
  }

  function pageSd() {
    text(4, 15, 'SD CARD', C.txt, true);
    text(204, 15, app.sd === 'ready' ? 'READY' : 'ERROR', app.sd === 'ready' ? C.green : C.red, true);

    const used = 1 - app.sdFree / app.sdTotal;
    bar(4, 26, 232, 8, used, C.acc);
    textf(4, 38, C.dim, false, 'USED %.1fG', app.sdTotal - app.sdFree);
    textf(180, 38, C.dim, false, 'FREE %.1fG', app.sdFree);

    text(4, 50, 'SDHC 32G  FAT32  CL10', C.txt);
    textf(4, 60, C.dim, false, 'READ %.1f MB/S  WRITE 0.9', app.sdRead);
    textf(4, 70, C.dim, false, 'ERR 0   STALL %d   RETRY 0', app.stalls);
    textf(4, 80, C.aqua, false, 'BUF %d%   STREAM 2/2', app.bufPct);
    textf(4, 90, C.faint, false, '%d KITS  -- PROJ  -- REC', KITS.length);

    chip(4, 104, 34, 12, 'TEST');
    chip(42, 104, 56, 12, 'REMOUNT');
    chip(102, 104, 50, 12, 'FORMAT', false, true);
    text(158, 107, 'ERASES CARD', C.faint);
    bottombar('ENT TEST  FN+DEL FORMAT  BKS BACK');
  }

  function pageKit() {
    text(4, 15, 'KITS', C.acc, true);
    text(44, 15, '/KITS', C.faint);
    textf(176, 15, C.faint, false, 'FREE %.1fG', app.sdFree);

    KITS.forEach((k, i) => {
      const y = 27 + i * 15;
      const sel = i === app.kitSel;
      if (sel) {
        rect(2, y - 2, 236, 14, C.line);
        rect(2, y - 2, 2, 14, C.acc);
      }
      text(8, y, k.name, sel ? C.acc : C.txt, sel);
      textf(126, y, C.dim, false, '%d/16', k.samples);
      textf(166, y, C.faint, false, '%.1fM', k.bytes);
      if (k.mode === 'MISS') text(200, y, 'MISS', C.red, true);
      else text(200, y, k.mode, k.mode === 'RAM' ? C.dim : C.aqua, true);
    });
    bottombar('ENT LOAD  BKS DEL  TAB BACK');
  }

  function pageNoSd() {
    const cx = 100, cy = 30;
    frame(cx, cy, 40, 28, C.dim);
    rect(cx + 4, cy + 4, 12, 4, C.dim);
    ctx.strokeStyle = C.red;
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(cx + 2.5, cy + 2.5); ctx.lineTo(cx + 37.5, cy + 25.5); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(cx + 37.5, cy + 2.5); ctx.lineTo(cx + 2.5, cy + 25.5); ctx.stroke();

    const s1 = 'NO SD CARD';
    text((W - tw(s1)) / 2, 66, s1, C.red, true);
    const s2 = 'INSERT A FAT32 CARD';
    text((W - tw(s2)) / 2, 78, s2, C.dim);
    const s3 = 'KITS AND SAMPLES LIVE ON SD';
    text((W - tw(s3)) / 2, 90, s3, C.faint);
    const s4 = 'MOUNT FAILED  ERR 0X0D';
    text((W - tw(s4)) / 2, 104, s4, '#605952');
    bottombar('ENT RETRY   SYS DIAG');
  }

  function pageLoading() {
    rect(0, 0, W, 12, C.bar);
    hline(0, 11, W, C.line);
    text(3, 2, 'LOADING', C.acc, true);
    text(204, 2, 'SD', C.amb);

    text(12, 22, app.loadingName, C.txt, true);
    text(12, 32, 'READING KIT FROM SD', C.faint);

    bar(12, 46, 216, 8, app.loadingPct / 100, C.acc);
    textf(12, 60, C.acc, true, '%d%%', app.loadingPct);
    textf(44, 60, C.dim, false, '%d / 16 PADS', Math.round(app.loadingPct * 16 / 100));
    text(170, 60, '1.6 MB/S', C.faint);
    text(12, 74, 'PRELOAD 6   STREAM 10', C.dim);
    text(170, 74, '4.2S LEFT', C.faint);
    text(12, 90, 'DO NOT REMOVE CARD', C.amb);
    bottombar('BKS CANCEL');
  }

  // ------------------------------------------------------------ render
  function render() {
    ctx.fillStyle = C.bg;
    ctx.fillRect(0, 0, W, H);

    if (app.page === 'LOADING') {
      pageLoading();
      return;
    }

    topbar();
    if (app.recording) { pageRec(); return; }
    switch (app.page) {
      case 'PAD': pagePad(); break;
      case 'SEQ': pageSeq(); break;
      case 'SMP': pageSmp(); break;
      case 'MIX': pageMix(); break;
      case 'SYS': pageSys(); break;
      case 'SD': pageSd(); break;
      case 'KIT': pageKit(); break;
      case 'NOSD': pageNoSd(); break;
    }
    toast();
  }

  // ------------------------------------------------------------ audio
  let actx = null, master = null, drums = [];
  let seqTimer = null, nextTime = 0, seqStep = 0;

  function ensureAudio() {
    if (!actx) {
      actx = new (window.AudioContext || window.webkitAudioContext)();
      master = actx.createGain();
      master.gain.value = app.volume / 100 * 0.9;
      master.connect(actx.destination);
      buildDrums();
    }
    if (actx.state === 'suspended') actx.resume();
  }

  function buildDrums() {
    drums = [];
    for (let type = 0; type < 8; type++) {
      const sr = actx.sampleRate;
      const dur = type === 4 ? 0.45 : 0.6;
      const n = Math.floor(sr * dur);
      const buf = actx.createBuffer(1, n, sr);
      const d = buf.getChannelData(0);
      let ph = 0, lp = 0;
      for (let i = 0; i < n; i++) {
        const t = i / sr;
        const noise = Math.random() * 2 - 1;
        let s = 0;
        switch (type) {
          case 0: {
            const f = 45 + 120 * Math.exp(-t * 32);
            ph += 2 * Math.PI * f / sr;
            s = Math.sin(ph) * Math.exp(-t * 8) + (t < 0.006 ? (1 - t / 0.006) * 0.6 : 0);
            break;
          }
          case 1:
            s = (noise * 0.75 + Math.sin(2 * Math.PI * 185 * t) * 0.35) * Math.exp(-t * 15);
            break;
          case 2:
            s = noise * Math.exp(-t * 11) * (0.55 + 0.45 * Math.abs(Math.sin(t * 90)));
            break;
          case 3:
            lp += (noise - lp) * 0.5;
            s = (noise - lp) * Math.exp(-t * 55);
            break;
          case 4:
            lp += (noise - lp) * 0.4;
            s = (noise - lp) * Math.exp(-t * 7);
            break;
          case 5: {
            const f = 90 + 90 * Math.exp(-t * 16);
            ph += 2 * Math.PI * f / sr;
            s = Math.sin(ph) * Math.exp(-t * 7);
            break;
          }
          case 6:
            s = (noise * 0.5 + Math.sin(2 * Math.PI * 820 * t) * 0.5) * Math.exp(-t * 40);
            break;
          default:
            lp += (noise - lp) * 0.6;
            s = (noise - lp) * Math.exp(-t * 14);
            break;
        }
        d[i] = Math.max(-1, Math.min(1, s * 0.9));
      }
      drums.push(buf);
    }
  }

  function playSample(buf, when, vel) {
    if (!actx || !buf) return;
    const src = actx.createBufferSource();
    src.buffer = buf;
    const g = actx.createGain();
    g.gain.value = Math.max(0.05, Math.min(1, vel / 127));
    src.connect(g); g.connect(master);
    src.start(when || actx.currentTime);
  }

  function playPad(p, when, vel) {
    playSample(p.buffer || drums[p.synth % 8], when, vel);
  }

  function triggerPad(i, vel = 112) {
    ensureAudio();
    const p = pads[i];
    if (p.kind === 'empty' || p.cache === '!') return;
    playPad(p, 0, vel / 127);
    padLevel[i] = 127;
    app.selected = i;
  }

  function swingOffset() {
    return (app.swing - 50) / 100 * (60 / app.bpm / 2);
  }

  function schedule() {
    if (!actx || !app.playing) return;
    while (nextTime < actx.currentTime + 0.12) {
      const step = seqStep;
      const t = nextTime + (step % 2 ? swingOffset() : 0);
      for (let tr = 0; tr < 8; tr++) {
        const s = pattern[tr][step];
        if (s.on && pads[tr].kind !== 'empty') {
          playPad(pads[tr], t, s.vel / 127);
          padLevel[tr] = s.vel;
        }
      }
      const ms = Math.max(0, (t - actx.currentTime) * 1000);
      setTimeout(() => { if (app.playing) app.playhead = step; }, ms);
      nextTime += 60 / app.bpm / 4;
      seqStep = (seqStep + 1) % 16;
    }
  }

  function startSeq() {
    ensureAudio();
    if (app.playing) return;
    app.playing = true;
    seqStep = 0;
    nextTime = actx.currentTime + 0.06;
    seqTimer = setInterval(schedule, 25);
    schedule();
  }

  function stopSeq() {
    app.playing = false;
    if (seqTimer) { clearInterval(seqTimer); seqTimer = null; }
  }

  // ------------------------------------------------------------ loading / SD
  function applyKit(k) {
    pads.forEach((p, i) => {
      p.name = NAMES[i];
      p.full = FULL[i];
      p.kind = (i >= k.samples) ? 'empty' : (i < 8 ? 'sample' : 'synth');
      p.cache = p.kind === 'empty' ? null : (i < 8 ? 'R' : 'S');
      p.synth = i % 8;
      p.buffer = null;
    });
    app.selected = 0;
  }

  function loadKit(i) {
    const k = KITS[i];
    app.kitSel = i;
    if (!k || k.samples === 0) { showToast('KIT EMPTY'); return; }
    app.page = 'LOADING';
    app.loadingName = k.name;
    app.loadingPct = 0;
    const t = setInterval(() => {
      app.loadingPct += 7;
      if (app.loadingPct >= 100) {
        clearInterval(t);
        app.loadingPct = 100;
        applyKit(k);
        app.page = 'PAD';
        showToast('KIT LOADED');
      }
    }, 55);
  }

  function showToast(s) {
    app.toast = s;
    app.toastUntil = performance.now() + 1600;
  }

  // ------------------------------------------------------------ sampling
  function makeVoxBuffer() {
    const sr = actx.sampleRate;
    const n = Math.floor(sr * 0.9);
    const b = actx.createBuffer(1, n, sr);
    const d = b.getChannelData(0);
    for (let i = 0; i < n; i++) {
      const t = i / sr;
      const env = Math.exp(-t * 3.2);
      const vib = 1 + 0.012 * Math.sin(2 * Math.PI * 5.5 * t);
      const noise = Math.random() * 2 - 1;
      const s = Math.sin(2 * Math.PI * 220 * t * vib) * 0.55
              + Math.sin(2 * Math.PI * 440 * t) * 0.22
              + noise * 0.05;
      d[i] = Math.max(-1, Math.min(1, s * env * 0.9));
    }
    return b;
  }

  let recTimer = null;

  function startRec() {
    if (app.recording) return;
    ensureAudio();
    stopSeq();
    app.recording = true;
    app.recPct = 0;
    recTimer = setInterval(() => {
      app.recPct += 2;
      if (app.recPct >= 100) finishRec();
    }, 40);
    showToast('RECORDING');
  }

  function finishRec() {
    if (recTimer) { clearInterval(recTimer); recTimer = null; }
    app.recording = false;
    app.recPct = 100;

    const p = pads[app.selected];
    p.kind = 'sample';
    p.name = 'REC1';
    p.full = 'REC_001';
    p.cache = 'R';
    p.buffer = makeVoxBuffer();
    app.page = 'PAD';
    showToast('SAMPLED -> PAD ' + (app.selected + 1));
  }

  function cancelRec() {
    if (recTimer) { clearInterval(recTimer); recTimer = null; }
    app.recording = false;
    app.recPct = 0;
    showToast('CANCELLED');
  }

  function encodeWav(buffer) {
    const ch = buffer.numberOfChannels;
    const sr = buffer.sampleRate;
    const len = buffer.length;
    const data = buffer.getChannelData(0);
    const bytes = 44 + len * 2;
    const ab = new ArrayBuffer(bytes);
    const v = new DataView(ab);
    const wstr = (off, s) => { for (let i = 0; i < s.length; i++) v.setUint8(off + i, s.charCodeAt(i)); };
    wstr(0, 'RIFF'); v.setUint32(4, bytes - 8, true); wstr(8, 'WAVE');
    wstr(12, 'fmt '); v.setUint32(16, 16, true); v.setUint16(20, 1, true);
    v.setUint16(22, 1, true); v.setUint32(24, sr, true);
    v.setUint32(28, sr * 2, true); v.setUint16(32, 2, true); v.setUint16(34, 16, true);
    wstr(36, 'data'); v.setUint32(40, len * 2, true);
    for (let i = 0; i < len; i++) {
      const s = Math.max(-1, Math.min(1, data[i]));
      v.setInt16(44 + i * 2, s < 0 ? s * 0x8000 : s * 0x7FFF, true);
    }
    return new Blob([ab], { type: 'audio/wav' });
  }

  function exportWav() {
    const p = pads[app.selected];
    if (!p.buffer) { showToast('NO SAMPLE ON PAD'); return; }
    const url = URL.createObjectURL(encodeWav(p.buffer));
    const a = document.createElement('a');
    a.href = url;
    a.download = 'REC_001.WAV';
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 4000);
    showToast('WAV DOWNLOADED');
  }

  async function importWav(file) {
    if (!file) return;
    ensureAudio();
    try {
      const ab = await file.arrayBuffer();
      const buf = await actx.decodeAudioData(ab);
      const p = pads[app.selected];
      p.kind = 'sample';
      p.buffer = buf;
      p.cache = 'R';
      p.name = file.name.replace(/\.[^.]+$/, '').slice(0, 4).toUpperCase() || 'IMP';
      p.full = file.name.replace(/\.[^.]+$/, '').slice(0, 12).toUpperCase() || 'IMPORT';
      showToast('IMPORTED -> PAD ' + (app.selected + 1));
    } catch (e) {
      showToast('IMPORT FAILED');
    }
  }

  // ------------------------------------------------------------ navigation
  function nextPage() {
    const order = ['PAD', 'SEQ', 'SMP', 'MIX', 'SYS', 'KIT'];
    const i = order.indexOf(app.page);
    app.page = order[(i + 1) % order.length];
  }

  function enterAction() {
    switch (app.page) {
      case 'PAD':
        startRec();
        break;
      case 'SEQ': {
        const s = pattern[app.cursorTrack][app.cursorStep];
        s.on ^= 1;
        break;
      }
      case 'MIX': showToast('MUTE TOGGLE'); break;
      case 'SYS': if (app.sd === 'ready') app.page = 'SD'; break;
      case 'SD': app.sdRead = 2.4; showToast('CARD OK · 2.4 MB/S'); break;
      case 'KIT': loadKit(app.kitSel); break;
      case 'NOSD': insertSd(); break;
      default: break;
    }
  }

  function backAction() {
    switch (app.page) {
      case 'SEQ': pattern[app.cursorTrack][app.cursorStep].on = 0; break;
      case 'SD': app.page = 'SYS'; break;
      case 'KIT': app.page = 'PAD'; break;
      case 'SMP': app.page = 'PAD'; break;
      default: break;
    }
  }

  function insertSd() {
    app.sd = 'ready';
    app.sdFree = 12.4;
    app.page = 'PAD';
    showToast('CARD READY');
  }

  function removeSd() {
    app.sd = 'none';
    app.page = 'NOSD';
    stopSeq();
    showToast('CARD REMOVED');
  }

  // ------------------------------------------------------------ input
  const PAD_KEYS = {
    '1': 0, '2': 1, '3': 2, '4': 3,
    'q': 4, 'w': 5, 'e': 6, 'r': 7,
    'a': 8, 's': 9, 'd': 10, 'f': 11,
    'z': 12, 'x': 13, 'c': 14, 'v': 15
  };

  function onClick(x, y) {
    if (y < 12) {
      const i = Math.floor((x - 3) / 24);
      if (i >= 0 && i < 6) app.page = TABS[i];
      return;
    }
    if (app.page === 'PAD') {
      if (!app.levelsMode && x >= 4 && x < 124 && y >= 15 && y < 123) {
        const c = Math.floor((x - 4) / 30), r = Math.floor((y - 15) / 27);
        if (c >= 0 && c < 4 && r >= 0 && r < 4) triggerPad(r * 4 + c);
        return;
      }
      if (app.levelsMode && x >= 4 && x < 124 && y >= 15 && y < 123) {
        const c = Math.floor((x - 4) / 30), r = Math.floor((y - 15) / 27);
        if (c >= 0 && c < 4 && r >= 0 && r < 4) {
          const i = r * 4 + c;
          app.selected = i;
          app.levelValue = 8 + Math.round(i * 119 / 15);
        }
        return;
      }
    }
    if (app.page === 'SEQ' && x >= 25 && x < 233 && y >= 24 && y < 120) {
      const c = Math.floor((x - 25) / 13), r = Math.floor((y - 24) / 12);
      if (c >= 0 && c < 16 && r >= 0 && r < 8) {
        pattern[r][c].on ^= 1;
        app.cursorTrack = r;
        app.cursorStep = c;
      }
      return;
    }
    if (app.page === 'KIT' && y >= 25 && y < 117) {
      const i = Math.floor((y - 25) / 15);
      if (i >= 0 && i < KITS.length) {
        app.kitSel = i;
        loadKit(i);
      }
      return;
    }
    if (app.page === 'NOSD') { insertSd(); return; }
    if (app.page === 'SD' && y >= 104 && y < 118) {
      if (x >= 4 && x < 38) { app.sdRead = 2.4; showToast('CARD OK · 2.4 MB/S'); }
      else if (x >= 42 && x < 98) { app.stalls = 0; showToast('REMOUNTED'); }
      else if (x >= 102 && x < 152) { showToast('FORMAT DISABLED'); }
    }
  }

  window.addEventListener('keydown', e => {
    if (e.repeat) return;
    const k = e.key;
    const lk = k.toLowerCase();

    if (k === ' ' || k === 'Tab' || k.startsWith('Arrow')) e.preventDefault();

    if (app.recording) {
      if (k === 'Backspace') cancelRec();
      return;
    }

    if (k === ' ') { app.playing ? stopSeq() : startSeq(); return; }
    if (k === 'Tab') { nextPage(); return; }
    if (k === 'Enter') { enterAction(); return; }
    if (k === 'Backspace') { backAction(); return; }

    if (k.startsWith('Arrow')) {
      const dy = k === 'ArrowDown' ? 1 : k === 'ArrowUp' ? -1 : 0;
      const dx = k === 'ArrowRight' ? 1 : k === 'ArrowLeft' ? -1 : 0;
      if (app.page === 'SEQ') {
        app.cursorTrack = (app.cursorTrack + dy + 8) % 8;
        app.cursorStep = (app.cursorStep + dx + 16) % 16;
      } else if (app.page === 'KIT') {
        if (dy) app.kitSel = (app.kitSel + dy + KITS.length) % KITS.length;
      } else if (app.page === 'MIX') {
        app.cursorTrack = (app.cursorTrack + dy + 8) % 8;
      } else if (app.page === 'SMP' || app.page === 'PAD') {
        app.selected = (app.selected + dx + 16) % 16;
      }
      return;
    }

    if (lk === 'o') { app.levelsMode = true; return; }

    if (e.shiftKey && ['!', '@', '#', '$'].includes(k)) {
      app.bank = ['!', '@', '#', '$'].indexOf(k);
      showToast('BANK ' + 'ABCD'[app.bank]);
      return;
    }

    if (PAD_KEYS[lk] !== undefined && !e.ctrlKey && !e.metaKey) {
      triggerPad(PAD_KEYS[lk]);
      return;
    }

    if (k === '-' || k === '=') {
      app.volume = Math.max(0, Math.min(100, app.volume + (k === '=' ? 5 : -5)));
      if (master) master.gain.value = app.volume / 100 * 0.9;
      document.getElementById('vol').value = app.volume;
      document.getElementById('volOut').value = app.volume + '%';
      return;
    }
  });

  window.addEventListener('keyup', e => {
    if (e.key.toLowerCase() === 'o') app.levelsMode = false;
  });

  cvs.addEventListener('click', e => {
    const r = cvs.getBoundingClientRect();
    onClick((e.clientX - r.left) * W / r.width, (e.clientY - r.top) * H / r.height);
    ensureAudio();
  });

  // ------------------------------------------------------------ side panel
  function bindPanel() {
    document.getElementById('btnPlay').onclick = () => { startSeq(); };
    document.getElementById('btnStop').onclick = () => { stopSeq(); };

    const bpm = document.getElementById('bpm');
    bpm.oninput = () => {
      app.bpm = +bpm.value;
      document.getElementById('bpmOut').value = app.bpm;
    };

    const sw = document.getElementById('swing');
    sw.oninput = () => {
      app.swing = +sw.value;
      document.getElementById('swOut').value = app.swing + '%';
    };

    const vol = document.getElementById('vol');
    vol.oninput = () => {
      app.volume = +vol.value;
      document.getElementById('volOut').value = app.volume + '%';
      if (master) master.gain.value = app.volume / 100 * 0.9;
    };

    document.querySelectorAll('#pageBtns .btn').forEach(b => {
      b.onclick = () => { app.page = b.dataset.page; };
    });

    const sdBtn = document.getElementById('btnSd');
    sdBtn.onclick = () => {
      if (app.sd === 'ready') removeSd(); else insertSd();
    };

    document.getElementById('btnLoad').onclick = () => loadKit(app.kitSel > -1 ? app.kitSel : 0);

    document.getElementById('btnRec').onclick = () => startRec();
    document.getElementById('btnExport').onclick = () => exportWav();

    const fileInput = document.getElementById('fileWav');
    document.getElementById('btnImport').onclick = () => fileInput.click();
    fileInput.onchange = () => {
      importWav(fileInput.files[0]);
      fileInput.value = '';
    };
  }

  function syncPanel() {
    const ready = app.sd === 'ready';
    const sdState = document.getElementById('sdState');
    const sdBtn = document.getElementById('btnSd');
    if (sdState) sdState.textContent = ready ? `READY · ${app.sdFree.toFixed(1)}G FREE` : 'NO CARD · ERR 0X0D';
    if (sdBtn) sdBtn.textContent = ready ? 'REMOVE CARD' : 'INSERT CARD';
    const play = document.getElementById('btnPlay');
    if (play) play.classList.toggle('on', app.playing);
    document.querySelectorAll('#pageBtns .btn').forEach(b => {
      b.classList.toggle('on', b.dataset.page === app.page);
    });
  }

  // ------------------------------------------------------------ loop
  let last = 0;
  function loop(ts) {
    requestAnimationFrame(loop);
    if (ts - last < 33) return;
    last = ts;

    for (let i = 0; i < 16; i++) padLevel[i] *= 0.86;
    app.playhead %= 16;
    render();
    syncPanel();
  }

  // ------------------------------------------------------------ boot
  function boot() {
    const hash = (location.hash || '').replace('#', '').toUpperCase();
    applyKit(KITS[0]);
    if (['PAD', 'SEQ', 'SMP', 'MIX', 'SYS', 'KIT', 'SD', 'NOSD'].includes(hash)) app.page = hash;
    if (hash === 'NOSD') app.sd = 'none';
    if (hash === 'LEVELS') {
      app.page = 'PAD';
      app.levelsMode = true;
      app.selected = 5;
      app.levelValue = 48;
    }
    if (hash === 'REC') {
      app.page = 'PAD';
      app.recording = true;
      app.recPct = 45;
    }
    bindPanel();
    document.getElementById('sdState').textContent = 'READY · 12.4G FREE';
    requestAnimationFrame(loop);
  }

  try {
    boot();
  } catch (e) {
    document.title = 'ERR: ' + e.message;
    console.error(e);
  }
})();
