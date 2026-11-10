/* SPDX-License-Identifier: GPL-3.0-only
 * DUB-BOX web emulator — the FM-1's DUB pages in the browser, driven by the
 * same integer DSP as the firmware (dub-core.js, validated bit-exact against
 * dub_core.h). Sources: the built-in dub beat, your microphone or an audio
 * file. The controls mirror the firmware: OCT- walks the pages, K1..K3 rule
 * them, SEL is the page's switch, REC sounds the siren. */
"use strict";

const LCDS_W = 240, LCDS_H = 240, SCALE = 2;
const lcd = document.getElementById("lcd");
lcd.width = LCDS_W * SCALE; lcd.height = LCDS_H * SCALE;
const cx = lcd.getContext("2d");
cx.imageSmoothingEnabled = false;
cx.scale(SCALE, SCALE);

const C = {
  black: "#000000", white: "#f2f2f2", gray: "#8a8a92", dim: "#3a3a40",
  green: "#1ecc70", red: "#ff2c34", yellow: "#ffc618", orange: "#ff8c19",
  blue: "#4d9bff", cyan: "#60c8ff", rowbg: "#161a18", barbg: "#161a18",
  line: "#1c1f23",
};

const dub = dub_new();
dub_init(dub);
let page = 0;                       /* DUB, SIREN, DELAY, ISO */
let sirLvl = 88, sirHeld = false, muted = false;
let isoSel = -1;                    /* the kill keyboard's last band, for the hint */

const PAGES = ["DUB", "SIREN", "DELAY", "ISOLATOR"];

/* ---- the little formatters, dub_ui.h's way ------------------------------- */
function fmtNum(v) { return String(v); }
function fmtDb(db) { return (db > 0 ? "+" : "") + String(db); }
function fmtHz(hz) {
  if (hz >= 1000) {
    const t = Math.trunc((hz + 50) / 100);
    return (t / 10 >= 10 ? String(Math.trunc(t / 10 / 10)) + String(Math.trunc(t / 10) % 10)
                         : String(Math.trunc(t / 10))) + "." + (t % 10) + "K";
  }
  return String(hz);
}
function cat(a, b) { return a + b; }

/* ---- the LCD ------------------------------------------------------------- */
function text(x, y, s, color, size, align) {
  cx.fillStyle = color;
  cx.font = (size || 10) + "px 'Barlow Semi Condensed', 'Arial Narrow', monospace";
  cx.textAlign = align || "left";
  cx.textBaseline = "top";
  cx.fillText(s, x, y);
}
function textW(s, size) {
  cx.font = (size || 10) + "px 'Barlow Semi Condensed', 'Arial Narrow', monospace";
  return cx.measureText(s).width;
}
function rect(x, y, w, h, color) { cx.fillStyle = color; cx.fillRect(x, y, w, h); }

function row(r, label, value, color, v, vmax) {
  const y = 44 + r * 42;
  text(6, y, label, C.gray, 11);
  text(234, y - 4, value, color, 16, "right");
  if (vmax) {
    rect(6, y + 24, 220, 5, C.barbg);
    rect(6, y + 24, Math.trunc(220 * v / vmax), 5, color);
  }
}
function drawLcd() {
  rect(0, 0, LCDS_W, LCDS_H, C.black);
  /* the status bar: the emulator's own badge */
  rect(3, 2, 34, 12, dub.on ? C.green : "transparent");
  rect(3, 2, 34, 1, dub.on ? C.green : C.dim); rect(3, 13, 34, 1, dub.on ? C.green : C.dim);
  rect(3, 2, 1, 12, dub.on ? C.green : C.dim); rect(36, 2, 1, 12, dub.on ? C.green : C.dim);
  text(8, 3, "WEB", dub.on ? C.black : C.dim, 10);
  rect(0, 17, LCDS_W, 1, C.line);
  text(6, 22, PAGES[page], C.cyan, 12);
  rect(0, 39, LCDS_W, 1, C.line);
  {
    let state = "", col = C.green;
    if (page === 0) { state = dub.on ? "ON" : "OFF"; col = dub.on ? C.green : C.red; }
    else if (page === 2 && !dub_tape.lvl) { state = "MUTED"; col = C.red; }
    else if (page === 1 && sirHeld) { state = "SOUND"; col = C.green; }
    if (state) text(234, 22, state, col, 10, "right");
  }

  if (page === 0) {
    const v = dub_filter_get();
    let fs;
    if (v === 64) { fs = "FLAT"; text(6, 170, "nothing filtered at FLAT", C.gray, 10); }
    else fs = (v < 64 ? "LP " : "HP ") + fmtHz(dub_filter_hz(v));
    row(0, "K1 FILTER", fs, v === 64 ? C.gray : C.yellow, v === 64 ? 0 : (v < 64 ? 64 - v : v - 64), 64);
    row(1, "K2 MIX", fmtNum(dub.isolator.mix), C.white, dub.isolator.mix, 127);
    row(2, "K3 SEND", dub_tape.send ? fmtNum(dub_tape.send) : "DRY", dub_tape.send ? C.green : C.gray, dub_tape.send, 127);
    text(6, 218, "SEL ON/OFF   HOME BACK", C.gray, 10);
  } else if (page === 1) {
    row(0, "K1 PITCH", cat(fmtNum(dub.siren.freq_hz), " HZ"), C.orange, dub.siren.freq_hz - 220, 980);
    row(1, "K2 RATE", cat(fmtNum(dub_siren_rate_hz(dub.siren.rate)), " HZ"), C.orange, dub_siren_rate_hz(dub.siren.rate) - 1, 15);
    row(2, "K3 LEVEL", fmtNum(sirLvl), sirLvl ? C.orange : C.gray, sirLvl, 127);
    text(6, 192, "ALGO", C.gray, 10);
    text(90, 188, DUB_SIR_TYPE_NAMES[dub.siren.type] + "  " + DUB_SIW_NAMES[dub.siren.wave], C.white, 14);
    text(6, 218, "SEL WAVE   REC SOUNDS", C.gray, 10);
  } else if (page === 2) {
    row(0, "K1 TIME", cat(fmtNum(dub_tape.ms), " MS"), C.blue, dub_tape.ms - 40, DUB_TAPE_MS_MAX - 40);
    row(1, "K2 FEEDBK", fmtNum(dub_tape.fb), C.white, dub_tape.fb, 127);
    row(2, "K3 LEVEL", fmtNum(dub_tape.lvl), dub_tape.lvl ? C.blue : C.red, dub_tape.lvl, 127);
    text(6, 192, "ALGO TONE", C.gray, 10);
    text(110, 188, fmtNum(dub_tape.tone), C.white, 14);
    text(6, 218, "TAPE ECHO   HOME BACK", C.gray, 10);
  } else {
    const names = ["K1 SUB", "K2 BASS", "K3 MIDS", "A4 TOPS"];
    for (let b = 0; b < 4; b++) {
      const p = dub.isolator.p[b];
      if (p.mute) row(b, names[b], "KILL", C.red, 0, 0);
      else row(b, names[b], cat(fmtDb(p.gain_db), " DB"), p.gain_db ? C.white : C.gray, p.gain_db + 24, 36);
    }
    text(6, 218, "KEYS Z X C V KILL   SEL LIFTS", C.gray, 10);
  }
}

/* ---- the audio ----------------------------------------------------------- */
let ctxA = null, spNode = null, srcNode = null, micStream = null, fileBuf = null;
let masterGain = null;
const NBLK = 256;
const lr = new Int32Array(2 * NBLK);
let sourceKind = "beat";

function toF32(out, i, v) {                 /* Q23 to float, headroom like the FM-1 */
  out[i] = v / 8388608.0;
}

function startAudio(kind) {
  stopAudio();
  ctxA = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 44100 });
  masterGain = ctxA.createGain();
  masterGain.gain.value = 0.85;
  spNode = ctxA.createScriptProcessor(4 * NBLK, 2, 2);
  spNode.onaudioprocess = (e) => {
    const inL = e.inputBuffer.getChannelData(0), inR = e.inputBuffer.numberOfChannels > 1 ? e.inputBuffer.getChannelData(1) : inL;
    const oL = e.outputBuffer.getChannelData(0), oR = e.outputBuffer.getChannelData(1);
    for (let off = 0; off < e.inputBuffer.length; off += NBLK) {
      for (let i = 0; i < NBLK; i++) {
        const l = inL[off + i] || 0, r = inR[off + i] || 0;
        lr[2 * i] = Math.max(-8388608, Math.min(8388607, Math.trunc(l * 8388608 * 0.9)));
        lr[2 * i + 1] = Math.max(-8388608, Math.min(8388607, Math.trunc(r * 8388608 * 0.9)));
      }
      dub_master_process(dub, lr, NBLK);
      for (let i = 0; i < NBLK; i++) {
        oL[off + i] = muted ? 0 : Math.max(-1, Math.min(1, lr[2 * i] / 8388608.0));
        oR[off + i] = muted ? 0 : Math.max(-1, Math.min(1, lr[2 * i + 1] / 8388608.0));
      }
    }
  };
  spNode.connect(masterGain);
  masterGain.connect(ctxA.destination);

  if (kind === "beat") {
    srcNode = ctxA.createBufferSource();
    srcNode.buffer = beatBuffer(ctxA);
    srcNode.loop = true;
    srcNode.connect(spNode);
    srcNode.start();
  } else if (kind === "mic") {
    navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false } })
      .then((st) => { micStream = st; srcNode = ctxA.createMediaStreamSource(st); srcNode.connect(spNode); })
      .catch(() => setStatus("microphone refused — wear headphones and retry"));
  } else if (kind === "file") {
    if (!fileBuf) { setStatus("pick an audio file first"); switchSource("beat"); return; }
    srcNode = ctxA.createBufferSource();
    srcNode.buffer = fileBuf; srcNode.loop = true;
    srcNode.connect(spNode); srcNode.start();
  }
}
function stopAudio() {
  if (srcNode) { try { srcNode.stop(); } catch (e) {} srcNode = null; }
  if (micStream) { micStream.getTracks().forEach((t) => t.stop()); micStream = null; }
  if (spNode) { spNode.onaudioprocess = null; try { spNode.disconnect(); } catch (e) {} spNode = null; }
  if (masterGain) { try { masterGain.disconnect(); } catch (e) {} masterGain = null; }
  if (ctxA) { ctxA.close(); ctxA = null; }
}

/* the built-in dub beat, synthesized: kick, rim, hats, bass, skank — no assets */
function beatBuffer(actx) {
  const SR = actx.sampleRate, BPM = 68, beat = SR * 60 / BPM, bars = 4;
  const n = Math.floor(beat * 4 * bars);
  const buf = actx.createBuffer(2, n, SR);
  const L = buf.getChannelData(0), R = buf.getChannelData(1);
  const add = (t0, f) => {                    /* f(sample index within the note) */
    const i0 = Math.floor(t0);
    for (let i = 0; i < n - i0; i++) {
      const v = f(i);
      if (v === null) break;
      L[i0 + i] += v[0]; R[i0 + i] += v[1];
    }
  };
  const kick = (t) => add(t, (i) => i > SR * 0.35 ? null : (() => {
    const e = Math.exp(-i / (SR * 0.09));
    const s = Math.sin(2 * Math.PI * (52 + 60 * Math.exp(-i / (SR * 0.02))) * i / SR);
    return [s * e * 0.9, s * e * 0.9];
  })());
  const rim = (t) => add(t, (i) => i > SR * 0.06 ? null : (() => {
    const e = Math.exp(-i / (SR * 0.012));
    const s = (Math.random() * 2 - 1) * 0.4 + Math.sin(2 * Math.PI * 1700 * i / SR) * 0.3;
    return [s * e * 0.5, s * e * 0.5];
  })());
  const hat = (t, g) => add(t, (i) => i > SR * 0.04 ? null : (() => {
    const e = Math.exp(-i / (SR * 0.008));
    const s = (Math.random() * 2 - 1);
    return [s * e * g * 0.16, s * e * g * 0.16];
  })());
  const bassN = (t, hz, len, g) => add(t, (i) => i > len ? null : (() => {
    const e = Math.min(1, i / (SR * 0.01)) * Math.exp(-i / (SR * len * 0.7));
    const ph = i / SR * hz;
    const s = Math.sin(2 * Math.PI * ph) * 0.7 + Math.sin(2 * Math.PI * ph * 2) * 0.2 + (Math.sin(2 * Math.PI * ph * 3) * 0.1);
    return [s * e * g, s * e * g];
  })());
  const skank = (t, hzs, len, g) => add(t, (i) => i > len ? null : (() => {
    const e = Math.exp(-i / (SR * len * 0.4));
    let s = 0;
    for (const h of hzs) s += Math.sin(2 * Math.PI * h * i / SR);
    s /= hzs.length;
    return [s * e * g * 0.7, s * e * g * 0.7];
  })());
  const A1 = 55, C2 = 65.41, D2 = 73.42, E2 = 82.41, G1 = 49;
  const Am = [220, 261.63, 329.63], Dm = [220, 293.66, 349.23], Gm = [196, 246.94, 293.66], Em = [196, 246.94, 293.66];
  for (let bar = 0; bar < bars; bar++) {
    const b0 = bar * beat * 4;
    const roots = [A1, D2, G1, E2], chords = [Am, Dm, Gm, Em];
    kick(b0); kick(b0 + 2 * beat + beat * 0.75);
    rim(b0 + beat); rim(b0 + 3 * beat);
    for (let e = 0; e < 8; e++) hat(b0 + e * beat / 2, e % 2 ? 0.6 : 1);
    const root = roots[bar];
    bassN(b0, root, beat * 1.4, 0.55);
    bassN(b0 + 2 * beat + beat * 0.5, root * 1.5, beat * 0.5, 0.45);
    bassN(b0 + 3.5 * beat, root, beat * 0.4, 0.5);
    skank(b0 + beat + beat / 2, chords[bar], beat * 0.35, 0.5);
    skank(b0 + 3 * beat + beat / 2, chords[bar], beat * 0.35, 0.5);
  }
  for (let i = 0; i < n; i++) { L[i] = Math.tanh(L[i] * 1.2); R[i] = Math.tanh(R[i] * 1.2); }
  return buf;
}

/* ---- the controls -------------------------------------------------------- */
const $ = (id) => document.getElementById(id);
function setStatus(s) { $("status").textContent = s; }

const knobDefs = {
  0: [ // DUB: filter 0..127, mix, send
    { get: () => dub_filter_get(), set: (v) => dub_filter_set(v) },
    { get: () => dub.isolator.mix, set: (v) => { dub.isolator.mix = v; } },
    { get: () => dub_tape.send, set: (v) => dub_tape_set_send(v) },
  ],
  1: [ // SIREN: pitch 220..1200, rate, level
    { get: () => Math.round((dub.siren.freq_hz - 220) * 127 / 980), set: (v) => dub_siren_set_freq(dub.siren, Math.round(220 + v * 980 / 127)) },
    { get: () => dub.siren.rate, set: (v) => dub_siren_set_rate(dub.siren, v) },
    { get: () => sirLvl, set: (v) => { sirLvl = v; if (sirHeld) dub_siren_trigger(dub.siren, sirLvl); } },
  ],
  2: [ // DELAY: ms, fb, lvl
    { get: () => Math.round((dub_tape.ms - 40) * 127 / (DUB_TAPE_MS_MAX - 40)), set: (v) => dub_tape_set_ms(Math.round(40 + v * (DUB_TAPE_MS_MAX - 40) / 127)) },
    { get: () => dub_tape.fb, set: (v) => dub_tape_set_fb(v) },
    { get: () => dub_tape.lvl, set: (v) => dub_tape_set_lvl(v) },
  ],
  3: [ // ISO gains -24..+12
    { get: () => (dub.isolator.p[0].gain_db + 24) * 127 / 36, set: (v) => bandGain(0, Math.round(v * 36 / 127) - 24) },
    { get: () => (dub.isolator.p[1].gain_db + 24) * 127 / 36, set: (v) => bandGain(1, Math.round(v * 36 / 127) - 24) },
    { get: () => (dub.isolator.p[2].gain_db + 24) * 127 / 36, set: (v) => bandGain(2, Math.round(v * 36 / 127) - 24) },
  ],
};
function bandGain(b, g) {
  const p = { ...dub.isolator.p[b] };
  p.gain_db = Math.max(-24, Math.min(12, g));
  dub_isolator_set_band(dub.isolator, b, p);
}
function refreshKnobs() {
  for (let k = 1; k <= 3; k++) {
    const el = $("k" + k), def = knobDefs[page][k - 1];
    const val = Math.round(def.get());
    el.value = val;
    $("k" + k + "v").textContent = val;
  }
  $("algo").textContent = page === 1 ? "TYPE: " + DUB_SIR_TYPE_NAMES[dub.siren.type]
    : page === 2 ? "TONE: " + dub_tape.tone
    : page === 3 ? "TOPS: " + (dub.isolator.p[3].mute ? "KILL" : fmtDb(dub.isolator.p[3].gain_db) + " DB")
    : "—";
}
for (let k = 1; k <= 3; k++) {
  $("k" + k).addEventListener("input", (e) => {
    knobDefs[page][k - 1].set(Number(e.target.value));
    refreshKnobs();
  });
}
$("page").addEventListener("click", () => { page = (page + 1) % 4; refreshKnobs(); });
$("algo").addEventListener("click", () => {
  if (page === 1) { dub_siren_set_type(dub.siren, (dub.siren.type + 1) % DUB_SIR_TYPES); }
  else if (page === 2) { dub_tape_set_tone(dub_tape.tone + 8 > 127 ? 0 : dub_tape.tone + 8); }
  else if (page === 3) { bandGain(3, dub.isolator.p[3].mute ? 0 : dub.isolator.p[3].gain_db + 3 > 12 ? -24 : dub.isolator.p[3].gain_db + 3); }
  refreshKnobs();
});
$("sel").addEventListener("click", () => {
  if (page === 0) dub.on = dub.on ? 0 : 1;
  else if (page === 1) dub_siren_set_wave(dub.siren, (dub.siren.wave + 1) % DUB_SIW_N);
  else if (page === 3) for (let b = 0; b < 4; b++) { const p = { ...dub.isolator.p[b] }; p.mute = 0; dub_isolator_set_band(dub.isolator, b, p); }
  refreshKnobs();
});
function bandKill(b) {
  const p = { ...dub.isolator.p[b] };
  p.mute = p.mute ? 0 : 1;
  dub_isolator_set_band(dub.isolator, b, p);
}
const killKeys = { KeyZ: 0, KeyX: 1, KeyC: 2, KeyV: 3 };
window.addEventListener("keydown", (e) => {
  if (e.repeat) return;
  if (page === 3 && killKeys[e.code] !== undefined) bandKill(killKeys[e.code]);
  if (e.code === "Space") { sirenHold(true); e.preventDefault(); }
});
window.addEventListener("keyup", (e) => { if (e.code === "Space") sirenHold(false); });
function sirenHold(down) {
  if (down) {
    if (!sirLvl) sirLvl = 88;
    dub_siren_trigger(dub.siren, sirLvl);
    sirHeld = true;
  } else { dub.siren.active = 0; sirHeld = false; }
  $("rec").classList.toggle("held", sirHeld);
}
const rec = $("rec");
["pointerdown"].forEach((ev) => rec.addEventListener(ev, () => sirenHold(true)));
["pointerup", "pointerleave", "pointercancel"].forEach((ev) => rec.addEventListener(ev, () => sirenHold(false)));
$("mute").addEventListener("click", () => { muted = !muted; $("mute").classList.toggle("held", muted); });

$("src").addEventListener("change", (e) => {
  sourceKind = e.target.value;
  if (sourceKind === "file") { $("file").click(); if (!fileBuf) { e.target.value = "beat"; sourceKind = "beat"; return; } }
  if (ctxA) startAudio(sourceKind);
  setStatus(sourceKind === "mic" ? "microphone live — wear headphones" : sourceKind === "beat" ? "the dub beat plays" : "your file loops");
});
$("file").addEventListener("change", (e) => {
  const f = e.target.files[0];
  if (!f) return;
  const fr = new FileReader();
  fr.onload = async () => {
    if (!ctxA) startAudio("beat");
    fileBuf = await ctxA.decodeAudioData(fr.result);
    if (sourceKind === "file") startAudio("file");
    setStatus("file loaded: " + f.name);
  };
  fr.readAsArrayBuffer(f);
});
$("go").addEventListener("click", () => {
  if (!ctxA) { startAudio(sourceKind); $("go").textContent = "STOP"; }
  else { stopAudio(); $("go").textContent = "PLAY"; }
});

/* ---- the frame loop ------------------------------------------------------ */
function frame() { drawLcd(); requestAnimationFrame(frame); }
refreshKnobs();
frame();
setStatus("press PLAY, turn the knobs — the DSP is the firmware's own code");
