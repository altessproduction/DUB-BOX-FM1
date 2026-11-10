/* SPDX-License-Identifier: GPL-3.0-only
 * DUB-BOX dub modules for the browser — a line-for-line port of the firmware's
 * dub_core.h (the same Q15/Q23 integer math, Math.imul for the 32-bit products;
 * every product here stays far below 2^53, so the doubles are exact and the
 * emulator sounds like the FM-1). Ported from sloop-DUB's dub.c by
 * altessproduction; the FM-1 erratum noinline helpers collapse into plain
 * functions in JS. Bus: interleaved stereo Int32 +-8388608 at 44.1 kHz. */
"use strict";

const DUB_FS = 44100;
const DUB_BANDS = 4, DUB_SUB = 0, DUB_BASS = 1, DUB_MIDS = 2, DUB_TOPS = 3;
const DUB_SIR_TYPE_NAMES = ["WAIL", "HILO", "RAMP", "SWEEP", "SPACE"];
const DUB_SIR_TYPES = 5;
const DUB_SIW_NAMES = ["SQR", "PLS", "SAW", "TRI", "SIN"];
const DUB_SIW_N = 5;
const DUB_TAPE_LEN = 32768, DUB_TAPE_MS_MAX = 740;
const DUB_WOW_INC = 2179696, DUB_FLT_INC = 19617261;
const DUB_WOW_DEP = 20 << 8, DUB_FLT_DEP = 4 << 8;

const DUB_EXP2 = [
  32768, 33486, 34219, 34968, 35734, 36516, 37316, 38133,
  38968, 39821, 40693, 41584, 42495, 43425, 44376, 45348,
  46341, 47356, 48393, 49452, 50535, 51642, 52773, 53928,
  55109, 56316, 57549, 58809, 60097, 61413, 62757, 64132];

function dub_clamp(x, lo, hi) { return x < lo ? lo : x > hi ? hi : x; }
/* the C helper multiplies in 64 bits and shifts arithmetically (floor); the
 * products here stay under 2^53, so doubles are exact — floor, never imul */
function dub_mul15(a, b) { return Math.floor((a * b) / 32768) | 0; }
function dub_sine(phase) {
  const q = phase >>> 30;
  let y = (phase >>> 15) & 0x7FFF;
  if (q & 1) y = 32767 - y;
  if (q & 2) y = -y;
  return (y * 2) | 0;
}
function dub_hz_inc(hz) { return ((hz * 4294967296) / DUB_FS) | 0; }
function dub_exp2(x) {                     /* x in Q15 octaves */
  const i = x >> 15;                       /* arithmetic: the floor, also for x < 0 */
  const f = ((x & 0x7FFFFFFF) & 32767) >> 10;
  return i >= 0 ? (DUB_EXP2[f] * (1 << i)) | 0 : (DUB_EXP2[f] >> -i) | 0;
}

/* ------------------------------------------------ the isolator (the ISO page) --- */
function dub_isolator_new() {
  const d = {
    xover: [90, 420, 2600],
    p: [], eq_fast: [0, 0, 0, 0], eq_slow: [0, 0, 0, 0], lp: [{ v: 0 }, { v: 0 }, { v: 0 }, { v: 0 }],
    enabled: 0, mix: 127,
  };
  const cuts = [90, 180, 1000, 4200];
  for (let i = 0; i < 4; i++)
    d.p[i] = { cutoff_hz: cuts[i], gain_db: 0, eq_freq: 64, eq_q: 64, eq_db: 0, mute: 0 };
  return d;
}
function dub_isolator_set_band(d, band, p) {
  if (band >= DUB_BANDS) return;
  d.p[band] = {
    cutoff_hz: p.cutoff_hz, gain_db: dub_clamp(p.gain_db, -24, 12) | 0,
    eq_freq: p.eq_freq, eq_q: dub_clamp(p.eq_q, 1, 127) | 0, eq_db: dub_clamp(p.eq_db, -12, 12) | 0,
    mute: p.mute ? 1 : 0,
  };
}
function dub_gain(db) {
  let g = 32767;
  if (db > 0) for (let i = 0; i < db; i++) g = Math.imul(g, 36700) >> 15;
  else for (let i = 0; i > db; i--) g = Math.imul(g, 29200) >> 15;
  return g | 0;
}
function dub_lowpass(x, state, hz) {
  /* one-pole Q15: a = 2*pi*fc/FS*32768 = fc*4.663 — the 90/420/2600 Hz bands */
  const a = dub_clamp((Math.imul(hz, 4663) / 1000) | 0, 1, 32700);
  state.v = (state.v + dub_mul15((x - state.v) | 0, a)) | 0;
  return state.v;
}
function dub_isolator_process(d, lr, n) {
  if (!d.enabled || !d.mix) return;
  let live = 0;
  for (let b = 0; b < DUB_BANDS; b++)
    if (d.p[b].gain_db || d.p[b].eq_db || d.p[b].mute) { live = 1; break; }
  if (!live) return;
  let muted = 0;
  for (let b = 0; b < DUB_BANDS; b++) if (d.p[b].mute) muted++;
  if (muted >= DUB_BANDS) {                 /* the dub kill is total */
    for (let i = 0; i < 2 * n; i++) lr[i] = 0;
    return;
  }
  for (let i = 0; i < n; i++) {
    const in_l = lr[2 * i], in_r = lr[2 * i + 1];
    const mono = (in_l + in_r) >> 1;
    const low = dub_lowpass(mono, d.lp[DUB_SUB], d.xover[0]);
    const low_mid = dub_lowpass((mono - low) | 0, d.lp[DUB_BASS], d.xover[1]);
    const mid = dub_lowpass((mono - low - low_mid) | 0, d.lp[DUB_MIDS], d.xover[2]);
    const bands = [low, low_mid, mid, (mono - low - low_mid - mid) | 0];
    let wet = 0;
    for (let b = 0; b < DUB_BANDS; b++) {
      let out = bands[b];
      const q = d.p[b].eq_q + 1;
      d.eq_fast[b] = (d.eq_fast[b] + Math.trunc((bands[b] - d.eq_fast[b]) / q)) | 0;
      d.eq_slow[b] = (d.eq_slow[b] + ((d.eq_fast[b] - d.eq_slow[b]) >> 4)) | 0;
      if (d.p[b].eq_db)
        out = (out + Math.trunc((bands[b] - d.eq_slow[b]) * d.p[b].eq_db / 24)) | 0;
      if (!d.p[b].mute) wet = (wet + dub_mul15(out, dub_gain(d.p[b].gain_db))) | 0;
    }
    lr[2 * i] = (in_l + (Math.imul((wet - in_l) | 0, d.mix) >> 7)) | 0;
    lr[2 * i + 1] = (in_r + (Math.imul((wet - in_r) | 0, d.mix) >> 7)) | 0;
  }
}

/* ------------------------------------------------ the DJ filter (DUB page) --- */
const dub_flt = { v: 64, amt: 0, amt_t: 0, hz: 16000, hz_t: 16000, lp: 1 };
const dub_flt_ch = [{ low: 0, band: 0 }, { low: 0, band: 0 }];
function dub_filter_set(v) {
  const was = dub_flt.v;
  dub_flt.v = dub_clamp(v, 0, 127) | 0;
  let d;
  if (dub_flt.v < 64) {
    d = Math.trunc(((63 - dub_flt.v) << 15) / 64);
    dub_flt.lp = 1;
    dub_flt.hz_t = Math.trunc((16000 * dub_exp2(Math.imul(-7, d))) / 32768);
  } else if (dub_flt.v > 64) {
    d = Math.trunc(((dub_flt.v - 64) << 15) / 63);
    dub_flt.lp = 0;
    dub_flt.hz_t = Math.trunc((30 * dub_exp2(Math.imul(7, d))) / 32768);
  } else {
    d = 0;
    dub_flt.hz_t = 16000;
  }
  dub_flt.amt_t = d | 0;
  if (dub_flt.v !== was && !dub_flt.amt_t) dub_flt.hz = dub_flt.hz_t;
}
function dub_filter_get() { return dub_flt.v; }
function dub_filter_hz(v) {
  if (v === 64) return 16000;
  if (v < 64) return Math.trunc((16000 * dub_exp2(Math.imul(-7, Math.trunc(((63 - v) << 15) / 64)))) / 32768);
  return Math.trunc((30 * dub_exp2(Math.imul(7, Math.trunc(((v - 64) << 15) / 63)))) / 32768);
}
function dub_filter_reset() {
  dub_flt.v = 64; dub_flt.amt = 0; dub_flt.amt_t = 0; dub_flt.hz = 16000; dub_flt.hz_t = 16000; dub_flt.lp = 1;
  dub_flt_ch[0].low = dub_flt_ch[0].band = 0;
  dub_flt_ch[1].low = dub_flt_ch[1].band = 0;
}
function dub_filter_process(lr, n) {
  if (!dub_flt.amt && !dub_flt.amt_t) return;
  if (dub_flt.amt !== dub_flt.amt_t) {
    let st = (dub_flt.amt_t - dub_flt.amt) >> 3;
    if (!st) st = dub_flt.amt_t > dub_flt.amt ? 1 : -1;
    dub_flt.amt = (dub_flt.amt + st) | 0;
  }
  if (dub_flt.hz !== dub_flt.hz_t) {
    let st = (dub_flt.hz_t - dub_flt.hz) >> 3;
    if (!st) st = dub_flt.hz_t > dub_flt.hz ? 16 : -16;
    dub_flt.hz = (dub_flt.hz + st) | 0;
  }
  let f = Math.trunc((dub_flt.hz * 306010) / 65536);
  if (f < 1) f = 1;
  if (f > 8000) f = 8000;
  const amt = dub_flt.amt, q = 29491;
  for (let i = 0; i < n; i++) {
    const inl = lr[2 * i], inr = lr[2 * i + 1];
    const c0 = dub_flt_ch[0], c1 = dub_flt_ch[1];
    const outl = (c0.low + dub_mul15(c0.band, f)) | 0;
    const outh = (inl - outl - dub_mul15(c0.band, q)) | 0;
    c0.band = (c0.band + dub_mul15(outh, f)) | 0;
    c0.low = outl;
    const outr = (c1.low + dub_mul15(c1.band, f)) | 0;
    const our = (inr - outr - dub_mul15(c1.band, q)) | 0;
    c1.band = (c1.band + dub_mul15(our, f)) | 0;
    c1.low = outr;
    if (dub_flt.lp) {
      lr[2 * i] = (inl + dub_mul15((outl - inl) | 0, amt)) | 0;
      lr[2 * i + 1] = (inr + dub_mul15((outr - inr) | 0, amt)) | 0;
    } else {
      lr[2 * i] = (inl + dub_mul15((outh - inl) | 0, amt)) | 0;
      lr[2 * i + 1] = (inr + dub_mul15((our - inr) | 0, amt)) | 0;
    }
  }
}

/* --------------------------------------------- the tape echo (the DLY page) --- */
const dub_tape = {
  buf: new Int16Array(DUB_TAPE_LEN),
  w: 0, ms: 400, fb: 70, tone: 50, lvl: 80, send: 0, siren_send: 0,
  cur: 0, wow: 0, fl: 0, w0: 0, dw: 0, send_g: 0, sir_g: 0,
  col: 2000 + 50 * 240, lp: 0,
};
function dub_tape_set_ms(ms) { dub_tape.ms = dub_clamp(ms, 40, DUB_TAPE_MS_MAX) | 0; }
function dub_tape_set_fb(fb) { dub_tape.fb = Math.min(fb | 0, 127); }
function dub_tape_set_tone(tone) { dub_tape.tone = Math.min(tone | 0, 127); dub_tape.col = 2000 + dub_tape.tone * 240; }
function dub_tape_set_lvl(lvl) { dub_tape.lvl = Math.min(lvl | 0, 127); }
function dub_tape_set_send(send) { dub_tape.send = Math.min(send | 0, 127); }
function dub_tape_set_siren_send(send) { dub_tape.siren_send = Math.min(send | 0, 127); }
function dub_tape_block(n) {
  const t = dub_tape;
  let dl_t = Math.trunc((t.ms * DUB_FS) / 1000);
  if (dl_t < 64) dl_t = 64;
  if (dl_t >= DUB_TAPE_LEN) dl_t = DUB_TAPE_LEN - 1;
  if (!t.cur) t.cur = dl_t;
  t.cur = (t.cur + ((dl_t - t.cur) >> 4)) | 0;
  if (t.cur > dl_t - 8 && t.cur < dl_t + 8) t.cur = dl_t;
  if (t.cur < 16) t.cur = 16;
  t.w0 = ((Math.imul(dub_sine(t.wow), DUB_WOW_DEP) >> 15) + (Math.imul(dub_sine(t.fl), DUB_FLT_DEP) >> 15)) | 0;
  t.wow = (t.wow + DUB_WOW_INC) >>> 0;
  t.fl = (t.fl + DUB_FLT_INC) >>> 0;
  const w1 = ((Math.imul(dub_sine(t.wow), DUB_WOW_DEP) >> 15) + (Math.imul(dub_sine(t.fl), DUB_FLT_DEP) >> 15)) | 0;
  t.dw = Math.trunc((w1 - t.w0) / n);
  {   /* the throws' gains, slewed block to block */
    let st, g = t.send * 258;
    st = (g - t.send_g) >> 3;
    if (!st && g !== t.send_g) st = g > t.send_g ? 1 : -1;
    t.send_g = (t.send_g + st) | 0;
    g = t.siren_send * 258;
    st = (g - t.sir_g) >> 3;
    if (!st && g !== t.sir_g) st = g > t.sir_g ? 1 : -1;
    t.sir_g = (t.sir_g + st) | 0;
  }
}
function dub_tape_step(in_s) {
  const t = dub_tape;
  const ro = ((t.cur << 8) + t.w0) | 0;
  t.w0 = (t.w0 + t.dw) | 0;
  const i0 = ro >>> 8;
  const s0 = t.buf[(t.w - i0) & (DUB_TAPE_LEN - 1)];
  const s1 = t.buf[(t.w - i0 - 1) & (DUB_TAPE_LEN - 1)];
  const x = (s0 + (Math.imul(s1 - s0, ro & 255) >> 8)) | 0;
  t.lp = (t.lp + dub_mul15((x - t.lp) | 0, t.col)) | 0;
  let fbp = t.lp;
  fbp = (fbp - (dub_mul15(dub_mul15(fbp, fbp), fbp) >> 2)) | 0;
  let wr = ((in_s >> 9) + dub_mul15(fbp, t.fb * 230)) | 0;
  if (wr > -2 && wr < 2) wr = 0;            /* the empty tail dies to exact silence */
  if (wr > 32767) wr = 32767;
  if (wr < -32768) wr = -32768;
  t.buf[t.w & (DUB_TAPE_LEN - 1)] = wr;
  t.w = (t.w + 1) >>> 0;
  return (Math.floor((x * 2) * (t.lvl * 258) / 32768) * 256) | 0;
}

/* ------------------------------------------------------ the siren (SIR page) --- */
function dub_siren_new() {
  return {
    phase: 0, lfo_phase: 0, phase_inc: dub_hz_inc(740), lfo_inc: dub_hz_inc(2),
    lfo_val: 0, lfo_target: 0, rnd: 0x1234567,
    level: 0, level_cur: 0, freq_hz: 740,
    depth: 96, rate: 8, type: 0, wave: 0, sym: 64, send: 0, active: 0,
  };
}
function dub_siren_trigger(s, level) { s.level = Math.min(level | 0, 127); s.active = s.level !== 0 ? 1 : 0; }
function dub_siren_rate_hz(rate) { return 1 + (((Math.min(rate | 0, 127)) * 15 + 63) / 127 | 0); }
function dub_siren_set_freq(s, hz) {
  if (hz < 220) hz = 220; else if (hz > 1200) hz = 1200;
  s.freq_hz = hz; s.phase_inc = dub_hz_inc(hz);
}
function dub_siren_set_rate(s, rate) { s.rate = Math.min(rate | 0, 127); s.lfo_inc = dub_hz_inc(dub_siren_rate_hz(s.rate)); }
function dub_siren_set_type(s, type) {
  s.type = type >= DUB_SIR_TYPES ? DUB_SIR_TYPES - 1 : type;
  if (s.type === 4 && s.lfo_val === 0) s.lfo_val = s.lfo_target;   /* SPACE picks up its hold */
}
function dub_siren_set_wave(s, wave) { s.wave = wave >= DUB_SIW_N ? DUB_SIW_N - 1 : wave; }
function dub_siren_set_sym(s, sym) { s.sym = Math.min(sym | 0, 127); }
function dub_siren_set_send(s, send) { s.send = Math.min(send | 0, 127); }
function dub_sir_shape(s, t) {
  const a = 32 + ((s.sym * 191) >> 7);
  switch (s.type) {
    case 1: return t < a ? 32767 : -32767;
    case 2: return t < a ? (-32767 + Math.trunc((t * 65534) / a)) | 0 : -32767;
    case 3: return t < a ? (32767 - Math.trunc((t * 65534) / a)) | 0 : -32767;
    default:
      return t < a ? (-32767 + Math.trunc((t * 65534) / a)) | 0
                   : (32767 - Math.trunc(((t - a) * 65534) / (256 - a))) | 0;
  }
}
function dub_sir_osc(s, ph) {
  switch (s.wave) {
    case 0: return (ph >>> 31) ? -32767 : 32767;
    case 1: return ((ph >>> 30) === 0) ? 32767 : -32767;
    case 2: return ((ph >>> 16) - 32768) | 0;
    case 3: {
      const x = ph >>> 16;
      const y = ((ph >>> 30) & 1) ? (65535 - x) : x;
      return y - 32768;
    }
    default: return (dub_sine(ph) >> 1) | 0;
  }
}
function dub_siren_tick(s) {
  if (s.type === 4) {
    if (s.lfo_phase < s.lfo_inc)
      s.lfo_target = (s.rnd = (Math.imul(s.rnd, 1664525) + 1013904223) >>> 0) >> 16;
    s.lfo_val = (s.lfo_val + ((s.lfo_target - s.lfo_val) >> (10 - (s.sym >> 5)))) | 0;
  } else
    s.lfo_val = dub_sir_shape(s, s.lfo_phase >>> 24);
  const lfo = s.lfo_val;
  const oct = dub_mul15(lfo, s.depth * 516);
  const inc = ((s.phase_inc * dub_exp2(oct)) / 32768) | 0;
  s.phase = (s.phase + inc) >>> 0;
  s.lfo_phase = (s.lfo_phase + s.lfo_inc) >>> 0;
  let lev = s.level_cur;
  const tgt = s.level;
  lev += tgt > lev ? 1 : tgt < lev ? -1 : 0;
  s.level_cur = lev & 0xFFFF;
  if (!s.active && lev === 0) return 0;
  return (dub_sir_osc(s, s.phase) * lev * 4) | 0;
}

/* ---------------------------------------------------- the DUB-BOX master bus --- */
function dub_new() {
  return {
    on: 0,
    isolator: dub_isolator_new(),
    siren: dub_siren_new(),
  };
}
function dub_init(d) {
  d.on = 0;
  const iso = d.isolator;
  iso.enabled = 1; iso.mix = 127; iso.xover = [90, 420, 2600];
  const cuts = [90, 180, 1000, 4200];
  for (let i = 0; i < 4; i++) {
    iso.p[i] = { cutoff_hz: cuts[i], gain_db: 0, eq_freq: 64, eq_q: 64, eq_db: 0, mute: 0 };
    iso.eq_fast[i] = iso.eq_slow[i] = 0; iso.lp[i] = { v: 0 };
  }
  Object.assign(d.siren, dub_siren_new());
  const t = dub_tape;
  t.buf.fill(0); t.w = 0; t.cur = 0; t.wow = t.fl = 0; t.w0 = t.dw = 0;
  t.send_g = t.sir_g = 0; t.lp = 0;
  t.ms = 400; t.fb = 70; t.tone = 50; t.lvl = 80; t.send = 0; t.siren_send = 0; t.col = 2000 + 50 * 240;
  dub_filter_reset();
}
function dub_master_process(d, lr, n) {
  const t = dub_tape, s = d.siren;
  if (d.on) {
    dub_isolator_process(d.isolator, lr, n);
    dub_filter_process(lr, n);
    if (t.lvl || t.fb || t.send || t.siren_send) {
      dub_tape_block(n);
      for (let i = 0; i < n; i++) {
        const l = lr[2 * i], r = lr[2 * i + 1];
        const y = dub_siren_tick(s);
        const mono = ((l + y) + (r + y)) >> 1;
        const tin = (dub_mul15(mono, t.send_g) + dub_mul15(y, t.sir_g)) | 0;
        const wet = dub_tape_step(tin);
        lr[2 * i] = (lr[2 * i] + y + wet) | 0;
        lr[2 * i + 1] = (lr[2 * i + 1] + y + wet) | 0;
      }
      return;
    }
  }
  if (s.active || s.level_cur) {
    for (let i = 0; i < n; i++) {
      const y = dub_siren_tick(s);
      lr[2 * i] = (lr[2 * i] + y) | 0;
      lr[2 * i + 1] = (lr[2 * i + 1] + y) | 0;
    }
  }
}
/* export for the page and the node validation */
if (typeof module !== "undefined") module.exports = {
  DUB_FS, DUB_BANDS, DUB_SUB, DUB_BASS, DUB_MIDS, DUB_TOPS, DUB_TAPE_LEN, DUB_TAPE_MS_MAX,
  DUB_SIR_TYPE_NAMES, DUB_SIR_TYPES, DUB_SIW_NAMES, DUB_SIW_N,
  dub_clamp, dub_new, dub_init, dub_master_process,
  dub_isolator_new, dub_isolator_set_band, dub_isolator_process,
  dub_filter_set, dub_filter_get, dub_filter_hz, dub_filter_process,
  dub_tape, dub_tape_set_ms, dub_tape_set_fb, dub_tape_set_tone, dub_tape_set_lvl,
  dub_tape_set_send, dub_tape_set_siren_send, dub_tape_block, dub_tape_step,
  dub_siren_new, dub_siren_trigger, dub_siren_rate_hz, dub_siren_set_freq, dub_siren_set_rate,
  dub_siren_set_type, dub_siren_set_wave, dub_siren_set_sym, dub_siren_set_send, dub_siren_tick,
};
