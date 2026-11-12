/* SPDX-License-Identifier: GPL-3.0-only
 * DUB-BOX dub modules: isolator, DJ filter, tape echo, siren.
 * Ported from sloop-DUB (https://github.com/altessproduction/sloop-dub-fm1,
 * GPL-3.0-only) to the fm1-audio-router's 24-bit master bus: the isolator and
 * the filter run on the Q23 samples as they come, the tape echo and the siren
 * keep sloop's Q15 internals where their sound lives. No sequencer, no synths:
 * the siren, the delay and the filters, as the DUB/SIREN/DELAY/ISO pages rule.
 * The bus is interleaved stereo int32, +-8388608, 44.1 kHz (FX_RATE). */
#ifndef DUB_CORE_H
#define DUB_CORE_H
#include <stdint.h>
#ifndef APP_BLOCK
#define APP_BLOCK 256u                  /* the render's frames (app.h's); a bare test's fallback */
#endif
#ifdef PLAT_HOST
#include <string.h>
#endif

#define DUB_FS 44100u                     /* sloop's FS: the same coefficient math */
#define DUB_Q15 32767
#define DUB_PI2 0x6487ED51u

enum { DUB_SUB = 0, DUB_BASS = 1, DUB_MIDS = 2, DUB_TOPS = 3, DUB_BANDS = 4 };
typedef struct {
    int16_t cutoff_hz;
    int8_t gain_db;
    uint8_t mute;
} dub_band_param_t;   /* sloop's per-band EQ is not on DUB-BOX's ISO page: gone,
                       * with its per-sample smoothers (code and CPU back) */
typedef struct {
    dub_band_param_t p[DUB_BANDS];
    int32_t gain_q15[DUB_BANDS];              /* the band's gain, Q15, set with the knob */
    int32_t xover[3];
    int32_t lp[DUB_BANDS];
    uint8_t enabled;
    uint8_t mix;
} dub_isolator_t;

/* the siren's LFO curves (TYPE): WAIL the slow up/down, HILO the two tone,
 * RAMP the yelp's charge and snap, SWEEP the fall, SPACE the sample and hold */
enum { DUB_SIR_WAIL = 0, DUB_SIR_HILO, DUB_SIR_RAMP, DUB_SIR_SWEEP, DUB_SIR_SPACE, DUB_SIR_TYPES };
static const char *const DUB_SIR_TYPE_NAMES[DUB_SIR_TYPES] = {"WAIL", "HILO", "RAMP", "SWEEP", "SPACE"};
/* the oscillator's curves (WAVE): the NJD's harsh square, a 25% pulse, the ramp, the soft ones */
enum { DUB_SIW_SQR = 0, DUB_SIW_PLS, DUB_SIW_SAW, DUB_SIW_TRI, DUB_SIW_SIN, DUB_SIW_N };
static const char *const DUB_SIW_NAMES[DUB_SIW_N] = {"SQR", "PLS", "SAW", "TRI", "SIN"};

typedef struct {
    uint32_t phase, lfo_phase, phase_inc, lfo_inc;
    int32_t lfo_val, lfo_target;
    uint32_t rnd;
    uint16_t level, level_cur;
    uint16_t freq_hz;
    uint8_t depth, rate, type, wave, sym, send, active;
} dub_siren_t;

static int32_t dub_clamp(int32_t x, int32_t lo, int32_t hi)
{
    return x < lo ? lo : x > hi ? hi : x;
}
static int32_t dub_mul15(int32_t a, int32_t b) __attribute__((noinline));
/* noinline: the FM-1's CPU fault (felucca-bsp docs/CPU.md) — a 64-bit multiply
 * inlined among loads can leave a register one bit off and crash the chip. The
 * out-of-line helper uses the product at once, the multiply completes first. */
static int32_t dub_mul15(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * b) >> 15);
}
static uint32_t dub_mul15u(uint32_t a, uint32_t b) __attribute__((noinline));
static uint32_t dub_mul15u(uint32_t a, uint32_t b)
{
    return (uint32_t)(((uint64_t)a * b) >> 15);
}
static int32_t dub_mulraw(int32_t a, int32_t b) __attribute__((noinline));
static int32_t dub_mulraw(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * b) >> 16);
}
static int32_t dub_sine(uint32_t phase)
{
    uint32_t q = phase >> 30;
    uint32_t x = (phase >> 15) & 0x7FFFu;
    int32_t y = (int32_t)x;
    if (q & 1u)
        y = 32767 - y;
    if (q & 2u)
        y = -y;
    return y * 2;                                /* *2, not <<1: y may be negative */
}
static uint32_t dub_hz_inc(uint32_t hz)
{
    return (uint32_t)(((uint64_t)hz << 32) / DUB_FS);
}

/* ---------------------------------------------- the isolator (the ISO page) --- */
static void dub_isolator_init(dub_isolator_t *d)
{
    static const int32_t xo[3] = {90, 420, 2600};
    static const int16_t cuts[4] = {90, 180, 1000, 4200};
    uint32_t i;
    d->enabled = 0;
    d->mix = 127;
    for (i = 0; i < 3; i++)
        d->xover[i] = xo[i];
    for (i = 0; i < DUB_BANDS; i++) {
        d->p[i].cutoff_hz = cuts[i];
        d->p[i].gain_db = 0;
        d->p[i].mute = 0;
        d->lp[i] = 0;
    }
}
static int32_t dub_gain(int8_t db);
static void dub_isolator_set_band(dub_isolator_t *d, uint32_t band, dub_band_param_t p)
{
    if (band >= DUB_BANDS)
        return;
    d->p[band] = p;
    d->p[band].gain_db = (int8_t)dub_clamp(d->p[band].gain_db, -24, 12);
    d->gain_q15[band] = dub_gain(d->p[band].gain_db);
}
static int32_t dub_gain(int8_t db)             /* once a knob turn, not a sample */
{
    int32_t g = 32767, i;
    if (db > 0)
        for (i = 0; i < db; i++)
            g = (g * 36700) >> 15;
    else
        for (i = 0; i > db; i--)
            g = (g * 29200) >> 15;
    return g;
}
static int32_t dub_lowpass(int32_t x, int32_t *state, int32_t hz)
{
    /* one-pole Q15: a = 2*pi*fc/FS*32768 = fc*4.663 — sloop's port had fc*0.048
     * (its crossovers sat ~100x under their labels, the bands held only DC);
     * DUB-BOX puts them where the ISO page says: 90 / 420 / 2600 Hz. */
    int32_t a = dub_clamp((hz * 4663) / 1000, 1, 32700);
    *state += dub_mul15(x - *state, a);
    return *state;
}
static void dub_isolator_process(dub_isolator_t *d, int32_t *lr, uint32_t n)
{
    uint32_t i, b, live = 0;
    if (!d->enabled || !d->mix)
        return;
    for (b = 0; b < DUB_BANDS; b++)            /* every knob at rest: the module stands
                                                * aside and the original passes untouched */
        if (d->p[b].gain_db || d->p[b].mute) {
            live = 1;
            break;
        }
    if (!live)
        return;
    {   /* every band killed: the dub kill is total, not sloop's -42 dB blend rest */
        uint32_t muted = 0;
        for (b = 0; b < DUB_BANDS; b++)
            if (d->p[b].mute)
                muted++;
        if (muted >= DUB_BANDS) {
            for (i = 0; i < 2 * n; i++)
                lr[i] = 0;
            return;
        }
    }
    for (i = 0; i < n; i++) {
        int32_t in_l = lr[2 * i], in_r = lr[2 * i + 1];
        int32_t mono = (in_l + in_r) >> 1;
        int32_t low = dub_lowpass(mono, &d->lp[DUB_SUB], d->xover[0]);
        int32_t low_mid = dub_lowpass(mono - low, &d->lp[DUB_BASS], d->xover[1]);
        int32_t mid = dub_lowpass(mono - low - low_mid, &d->lp[DUB_MIDS], d->xover[2]);
        int32_t bands[DUB_BANDS];
        int32_t wet = 0;
        bands[DUB_SUB] = low;
        bands[DUB_BASS] = low_mid;
        bands[DUB_MIDS] = mid;
        bands[DUB_TOPS] = mono - low - low_mid - mid;
        for (b = 0; b < DUB_BANDS; b++)
            if (!d->p[b].mute)
                wet += dub_mul15(bands[b], d->gain_q15[b]);
        lr[2 * i] = in_l + (((wet - in_l) * d->mix) >> 7);
        lr[2 * i + 1] = in_r + (((wet - in_r) * d->mix) >> 7);
    }
}

/* ------------------------------------------------ the DJ filter (DUB page) --- */
/* Bipolar: 0..63 lowpass (deeper the lower), 64 neutral (flat, no filtering),
 * 65..127 highpass. One state-variable filter a channel, its amount and cutoff
 * slewed so sweeps don't click. */
typedef struct { int32_t low, band; } dub_svf_t;
static struct {
    int16_t v;
    int32_t amt, amt_t, hz, hz_t, lp;
} dub_flt = {64, 0, 0, 16000, 16000, 1};
static dub_svf_t dub_flt_ch[2];

/* 2^(i/32) in Q15, i = 0..31: the exponential sweeps' fractional table */
static const int32_t DUB_EXP2[32] = {
    32768, 33486, 34219, 34968, 35734, 36516, 37316, 38133,
    38968, 39821, 40693, 41584, 42495, 43425, 44376, 45348,
    46341, 47356, 48393, 49452, 50535, 51642, 52773, 53928,
    55109, 56316, 57549, 58809, 60097, 61413, 62757, 64132
};
/* 2^x, x in Q15 octaves */
static int32_t dub_exp2(int32_t x)
{
    int32_t i = x >> 15;                           /* arithmetic shift: the floor, also for x < 0 */
    uint32_t f = ((uint32_t)x & 32767u) >> 10;     /* the fraction, 0..31 (no negative left shift) */
    return i >= 0 ? (int32_t)DUB_EXP2[f] << i : (int32_t)DUB_EXP2[f] >> -i;
}

static void dub_filter_set(int16_t v)
{
    int32_t d;
    int16_t was = dub_flt.v;
    dub_flt.v = (int16_t)dub_clamp(v, 0, 127);
    if (dub_flt.v < 64) {
        d = ((int32_t)(63 - dub_flt.v) << 15) / 64;
        dub_flt.lp = 1;
        dub_flt.hz_t = (16000 * dub_exp2(-7 * d)) >> 15;
    } else if (dub_flt.v > 64) {
        d = ((int32_t)(dub_flt.v - 64) << 15) / 63;
        dub_flt.lp = 0;
        dub_flt.hz_t = (30 * dub_exp2(7 * d)) >> 15;
    } else {
        d = 0;
        dub_flt.hz_t = 16000;
    }
    dub_flt.amt_t = d;
    if (dub_flt.v != was && !dub_flt.amt_t)
        dub_flt.hz = dub_flt.hz_t;
}
static int16_t dub_filter_get(void) { return dub_flt.v; }
static uint32_t dub_filter_hz(int16_t v)
{
    int32_t d;
    if (v == 64)
        return 16000;
    if (v < 64) {
        d = ((int32_t)(63 - v) << 15) / 64;
        return (uint32_t)((16000 * dub_exp2(-7 * d)) >> 15);
    }
    d = ((int32_t)(v - 64) << 15) / 63;
    return (uint32_t)((30 * dub_exp2(7 * d)) >> 15);
}
static void dub_filter_process(int32_t *lr, uint32_t n)
{
    uint32_t i;
    int32_t amt, f, q = 29491;
    if (!dub_flt.amt && !dub_flt.amt_t)
        return;
    if (dub_flt.amt != dub_flt.amt_t) {
        int32_t st = (dub_flt.amt_t - dub_flt.amt) >> 3;
        if (!st)
            st = dub_flt.amt_t > dub_flt.amt ? 1 : -1;
        dub_flt.amt += st;
    }
    if (dub_flt.hz != dub_flt.hz_t) {
        int32_t st = (dub_flt.hz_t - dub_flt.hz) >> 3;
        if (!st)
            st = dub_flt.hz_t > dub_flt.hz ? 16 : -16;
        dub_flt.hz += st;
    }
    f = dub_mulraw(dub_flt.hz, 306010);   /* 2 sin(pi fc/fs) ~ 2 pi fc/fs = hz*205887/44100, hz*306010>>16, noinline */
    if (f < 1)
        f = 1;
    if (f > 8000)
        f = 8000;
    amt = dub_flt.amt;
    for (i = 0; i < n; i++) {
        int32_t inl = lr[2 * i], inr = lr[2 * i + 1], outl, outh, outr, our;
        outl = dub_flt_ch[0].low + dub_mul15(dub_flt_ch[0].band, f);
        outh = inl - outl - dub_mul15(dub_flt_ch[0].band, q);
        dub_flt_ch[0].band += dub_mul15(outh, f);
        dub_flt_ch[0].low = outl;
        outr = dub_flt_ch[1].low + dub_mul15(dub_flt_ch[1].band, f);
        our = inr - outr - dub_mul15(dub_flt_ch[1].band, q);
        dub_flt_ch[1].band += dub_mul15(our, f);
        dub_flt_ch[1].low = outr;
        if (dub_flt.lp) {
            lr[2 * i] = inl + dub_mul15(outl - inl, amt);
            lr[2 * i + 1] = inr + dub_mul15(outr - inr, amt);
        } else {
            lr[2 * i] = inl + dub_mul15(outh - inl, amt);
            lr[2 * i + 1] = inr + dub_mul15(our - inr, amt);
        }
    }
}

/* --------------------------------------------- the tape echo (the DLY page) --- */
/* The DLY page's tape delay, standalone: the time glides to its target (the
 * tape's sag), the read head wanders (wow 0.7 Hz deep, flutter 6.3 Hz shallow,
 * ramped across the block) and the repeats pass the feedback's damping and a
 * soft clip (x - x^3 / 4), warming pass by pass. The buffer is 32768 samples
 * of Q15 wet: about 743 ms of tape. The input is the throw (the module's
 * output, DUB page's SEND) plus the siren's send. */
#define DUB_TAPE_LEN 32768u
#define DUB_TAPE_MS_MAX 740u
#define DUB_WOW_INC 2179696u    /* 0.7 Hz, a phase step a block */
#define DUB_FLT_INC 19617261u   /* 6.3 Hz */
#define DUB_WOW_DEP (20 << 8)   /* +- 0.45 ms, Q8 samples */
#define DUB_FLT_DEP (4 << 8)    /* +- 0.09 ms */
/* The tape's buffer is NOT static: 64 KiB of .bss would eat the work area the
 * waveform/spectrum captures need (98,304 B). Foreground code holds it in the
 * work area while the echo is in use and lets the displays take it back
 * (dub_tape_hold / dub_tape_wanted); without a buffer the echo passes silence
 * and the dry path is bit-transparent. Tests attach their own storage. */
static int16_t *volatile dub_tape_buf;
static uint32_t dub_tape_w;
static uint16_t dub_tape_ms = 400, dub_tape_fb = 70, dub_tape_tone = 50, dub_tape_lvl = 80;
static uint8_t dub_tape_send = 0;                   /* the DUB page's throw, 0 DRY */
static int32_t dub_tape_cur;                        /* the gliding read time, samples */
static uint32_t dub_tape_wow, dub_tape_fl;
static int32_t dub_tape_w0, dub_tape_dw;
static int32_t dub_tape_send_g, dub_tape_sir_g;     /* the throws' gains, Q15, slewed */
static int32_t dub_tape_col = 2000 + 50 * 240;      /* the repeats' damping, slewless */
static int32_t dub_tape_lp;                         /* the feedback's one-pole */
static uint8_t dub_siren_send = 48;                 /* the siren's own throw, 0 DRY — the tape
                                                     * reads its own copy (dub_tape_set_siren_send);
                                                     * the SIREN page's K4 SEND rules both */

/* The sources' throws (the DUB page's K3 USB SEND and K4 BT SEND, the 9th and
 * 10th black keys' arms): the tape reads the USB in and the BT in as they came
 * in — post-fader, before the module's isolator and filter touch the bus — two
 * throws the page sets and the keys arm, both armed with their levels at 0 at
 * boot (the send delay is active, its send at 0). The USB throw arrives per
 * block from the render (uac_mix's dub_send_usb_capture); the BT throw is the
 * mix minus that USB throw, taken before the filters. A block the render did
 * not capture throws silence: a send rests, it does not lie with a stale bus. */
static uint8_t dub_send_bt = 0;                     /* the BT in throw's level, 0 DRY */
static uint8_t dub_send_usb_on = 1, dub_send_bt_on = 1;   /* the black keys' arms, on at boot */
static int32_t dub_send_bt_g;                       /* the BT throw's gain, Q15, slewed */
static int16_t dub_throw_usb[APP_BLOCK], dub_throw_bt[APP_BLOCK];
static volatile uint8_t dub_sends_live;             /* the render left a fresh USB throw */

static void dub_tape_init(void)
{
    dub_tape_buf = 0;
    dub_tape_w = 0;
    dub_tape_cur = 0;
    dub_tape_wow = dub_tape_fl = 0;
    dub_tape_w0 = dub_tape_dw = 0;
    dub_tape_send_g = dub_tape_sir_g = 0;
    dub_tape_lp = 0;
}
static uint32_t dub_tape_bytes(void) { return DUB_TAPE_LEN * 2u; }
/* foreground only: attach/detach the tape's storage; the audio side picks the
 * pointer up whole (one aligned word), a stale buffer is a valid buffer */
static void dub_tape_attach(int16_t *buf)
{
    dub_tape_buf = buf;
    if (buf) {
        memset(buf, 0, DUB_TAPE_LEN * 2u);
        dub_tape_w = 0;
        dub_tape_cur = 0;
        dub_tape_lp = 0;
    }
}
static void dub_tape_set_ms(uint16_t ms)
{
    dub_tape_ms = ms < 40u ? 40u : ms > DUB_TAPE_MS_MAX ? DUB_TAPE_MS_MAX : ms;
}
static void dub_tape_set_fb(uint8_t fb) { dub_tape_fb = fb > 127u ? 127u : fb; }
static void dub_tape_set_tone(uint8_t tone)
{
    dub_tape_tone = tone > 127u ? 127u : tone;
    dub_tape_col = 2000 + dub_tape_tone * 240;
}
static void dub_tape_set_lvl(uint8_t lvl) { dub_tape_lvl = lvl > 127u ? 127u : lvl; }
static void dub_tape_set_send(uint8_t send) { dub_tape_send = send > 127u ? 127u : send; }
static void dub_tape_set_bt_send(uint8_t send) { dub_send_bt = send > 127u ? 127u : send; }
static void dub_tape_set_siren_send(uint8_t send) { dub_siren_send = send > 127u ? 127u : send; }

/* the render's USB in throw (uac_mix calls it with the USB in bus, after the
 * app's faders, before the Bluetooth mix): the mono bus in Q15 for the tape */
void dub_send_usb_capture(const int32_t *lr, uint32_t n)
{
    uint32_t i;
    if (n > APP_BLOCK) {
        dub_sends_live = 0;
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t m = (lr[2 * i] + lr[2 * i + 1]) >> 9;    /* the bus's mono, Q23 -> Q15 */
        if (m > 32767)
            m = 32767;
        else if (m < -32768)
            m = -32768;
        dub_throw_usb[i] = (int16_t)m;
    }
    dub_sends_live = 1;
}

/* whether the tape turns at all: the echo's own level and feedback keep a tail
 * ringing, and any throw connected — the siren's, or an armed source's send */
static uint8_t dub_tape_runs(void)
{
    return dub_tape_lvl || dub_tape_fb || dub_siren_send ||
           (dub_send_usb_on && dub_tape_send) || (dub_send_bt_on && dub_send_bt);
}

/* per block: the read time and the offset's ramp */
static void dub_tape_block(uint32_t n)
{
    int32_t dl_t = (int32_t)(((int64_t)dub_tape_ms * DUB_FS) / 1000);
    int32_t w1;
    if (dl_t < 64)
        dl_t = 64;                                   /* the wow needs room behind the head */
    if (dl_t >= (int32_t)DUB_TAPE_LEN)
        dl_t = (int32_t)DUB_TAPE_LEN - 1;
    if (!dub_tape_cur)
        dub_tape_cur = dl_t;
    dub_tape_cur += (dl_t - dub_tape_cur) >> 4;      /* the tape's sag: the time glides */
    if (dub_tape_cur > dl_t - 8 && dub_tape_cur < dl_t + 8)
        dub_tape_cur = dl_t;
    if (dub_tape_cur < 16)
        dub_tape_cur = 16;
    dub_tape_w0 = ((dub_sine(dub_tape_wow) * DUB_WOW_DEP) >> 15) + ((dub_sine(dub_tape_fl) * DUB_FLT_DEP) >> 15);
    dub_tape_wow += DUB_WOW_INC;                     /* one step a block: w1 is the next block's start */
    dub_tape_fl += DUB_FLT_INC;
    w1 = ((dub_sine(dub_tape_wow) * DUB_WOW_DEP) >> 15) + ((dub_sine(dub_tape_fl) * DUB_FLT_DEP) >> 15);
    dub_tape_dw = (w1 - dub_tape_w0) / (int32_t)n;
    {   /* the throws' gains, slewed block to block: the module output's throw is
         * gone — the USB in (K3, the 9th key's arm) and the BT in (K4, the 10th)
         * throw instead, each silenced while its key lifts the arm */
        int32_t st, t;
        t = (int32_t)(dub_send_usb_on ? dub_tape_send : 0) * 258;
        st = (t - dub_tape_send_g) >> 3;
        if (!st && t != dub_tape_send_g)
            st = t > dub_tape_send_g ? 1 : -1;
        dub_tape_send_g += st;
        t = (int32_t)(dub_send_bt_on ? dub_send_bt : 0) * 258;
        st = (t - dub_send_bt_g) >> 3;
        if (!st && t != dub_send_bt_g)
            st = t > dub_send_bt_g ? 1 : -1;
        dub_send_bt_g += st;
        t = (int32_t)dub_siren_send * 258;
        st = (t - dub_tape_sir_g) >> 3;
        if (!st && t != dub_tape_sir_g)
            st = t > dub_tape_sir_g ? 1 : -1;
        dub_tape_sir_g += st;
    }
}
/* one sample in (Q23, the throw already summed), the wet out (Q23, leveled) */
static int32_t dub_tape_step(int32_t in)
{
    int32_t ro, x, fbp, wet, wr;
    uint32_t i0;
    if (!dub_tape_buf)
        return 0;                                    /* no storage: the echo rests */
    ro = (dub_tape_cur << 8) + dub_tape_w0;
    dub_tape_w0 += dub_tape_dw;
    i0 = (uint32_t)ro >> 8;
    {
        int32_t s0 = dub_tape_buf[(dub_tape_w - i0) & (DUB_TAPE_LEN - 1u)];
        int32_t s1 = dub_tape_buf[(dub_tape_w - i0 - 1u) & (DUB_TAPE_LEN - 1u)];
        x = s0 + ((s1 - s0) * (ro & 255) >> 8);
    }
    dub_tape_lp += dub_mul15(x - dub_tape_lp, dub_tape_col);
    fbp = dub_tape_lp;                               /* the tape's soft clip (x - x^3 / 4, Q15):
                                                      * the repeats warm up pass by pass */
    fbp -= dub_mul15(dub_mul15(fbp, fbp), fbp) >> 2;
    wr = (in >> 9) + dub_mul15(fbp, dub_tape_fb * 230);
    /* The arithmetic >> floors negatives, so the feedback alone would recirculate
     * a +-1 LSB truncation whisper forever (mul15(-1, x) = -1). A real tape's
     * noise floor sits far above -138 dB: let the empty tail die to exact
     * silence, so the chain at rest (no throw connected) is bit-transparent. */
    if (wr > -2 && wr < 2)
        wr = 0;
    if (wr > 32767)
        wr = 32767;
    if (wr < -32768)
        wr = -32768;
    dub_tape_buf[dub_tape_w & (DUB_TAPE_LEN - 1u)] = (int16_t)wr;
    dub_tape_w++;
    wet = dub_mul15(x * 2, dub_tape_lvl * 258) * 256;   /* the wet level, Q15 hot x2, back to Q23 */
    return wet;
}

/* ------------------------------------------------------ the siren (SIR page) --- */
static void dub_siren_init(dub_siren_t *s)
{
    s->phase = 0;
    s->lfo_phase = 0;
    s->phase_inc = dub_hz_inc(740);
    s->lfo_inc = dub_hz_inc(2);
    s->lfo_val = 0;
    s->lfo_target = 0;
    s->rnd = 0x1234567u;
    s->level = 0;
    s->level_cur = 0;
    s->freq_hz = 740;
    s->depth = 96;
    s->rate = 8;
    s->type = DUB_SIR_WAIL;
    s->wave = DUB_SIW_SQR;
    s->sym = 64;
    s->send = 48;                                /* the SIREN page's K4 SEND: sloop's dubby
                                                  * echo — the siren feeds the tape from the
                                                  * start, its tail rings after the gate opens */
    s->active = 0;
}
static void dub_siren_trigger(dub_siren_t *s, uint8_t level)
{
    s->level = level > 127 ? 127 : level;
    s->active = s->level != 0;
}
static uint32_t dub_siren_rate_hz(uint8_t rate)
{
    return 1u + ((uint32_t)(rate > 127u ? 127u : rate) * 15u + 63u) / 127u;
}
static void dub_siren_set_freq(dub_siren_t *s, uint16_t hz)
{
    if (hz < 220u)
        hz = 220u;
    else if (hz > 1200u)
        hz = 1200u;
    s->freq_hz = hz;
    s->phase_inc = dub_hz_inc(hz);
}
static void dub_siren_set_rate(dub_siren_t *s, uint8_t rate)
{
    s->rate = rate > 127u ? 127u : rate;
    s->lfo_inc = dub_hz_inc(dub_siren_rate_hz(s->rate));
}
static void dub_siren_set_depth(dub_siren_t *s, uint8_t depth) { s->depth = depth > 127u ? 127u : depth; }
static void dub_siren_set_type(dub_siren_t *s, uint8_t type)
{
    s->type = type >= DUB_SIR_TYPES ? (uint8_t)(DUB_SIR_TYPES - 1u) : type;
    if (s->type == DUB_SIR_SPACE && s->lfo_val == 0)
        s->lfo_val = s->lfo_target;
}
static void dub_siren_set_wave(dub_siren_t *s, uint8_t wave)
{
    s->wave = wave >= DUB_SIW_N ? (uint8_t)(DUB_SIW_N - 1u) : wave;
}
static void dub_siren_set_sym(dub_siren_t *s, uint8_t sym) { s->sym = sym > 127u ? 127u : sym; }
static void dub_siren_set_send(dub_siren_t *s, uint8_t send) { s->send = send > 127u ? 127u : send; }

/* the LFO's curve at t = 0..255: every type has its symmetry's say */
static int32_t dub_sir_shape(dub_siren_t *s, uint32_t t)
{
    uint32_t a = 32u + ((uint32_t)s->sym * 191u >> 7);
    switch (s->type) {
    case DUB_SIR_HILO:
        return t < a ? 32767 : -32767;
    case DUB_SIR_RAMP:
        return t < a ? (int32_t)(-32767 + (int32_t)((t * 65534u) / a)) : -32767;
    case DUB_SIR_SWEEP:
        return t < a ? (int32_t)(32767 - (int32_t)((t * 65534u) / a)) : -32767;
    default:
        return t < a ? (int32_t)(-32767 + (int32_t)((t * 65534u) / a))
                     : (int32_t)(32767 - (int32_t)(((t - a) * 65534u) / (256u - a)));
    }
}
static int32_t dub_sir_osc(dub_siren_t *s, uint32_t ph)
{
    switch (s->wave) {
    case DUB_SIW_SQR:
        return ph >> 31 ? -32767 : 32767;
    case DUB_SIW_PLS:
        return (ph >> 30) == 0u ? 32767 : -32767;
    case DUB_SIW_SAW:
        return (int32_t)(ph >> 16) - 32768;
    case DUB_SIW_TRI: {
        uint32_t x = ph >> 16;
        int32_t y = (ph >> 30) & 1u ? (int32_t)(65535u - x) : (int32_t)x;
        return y - 32768;
    }
    default:
        return dub_sine(ph) >> 1;
    }
}
/* one sample of the siren, Q23, the same hot scale sloop's siren had (0 silent) */
static int32_t dub_siren_tick(dub_siren_t *s)
{
    int32_t lfo, oct, lev, y;
    uint32_t inc;
    if (s->type == DUB_SIR_SPACE) {
        if (s->lfo_phase < s->lfo_inc)
            s->lfo_target = (int32_t)(s->rnd = s->rnd * 1664525u + 1013904223u) >> 16;
        s->lfo_val += (s->lfo_target - s->lfo_val) >> (10u - (uint32_t)(s->sym >> 5));
    } else
        s->lfo_val = dub_sir_shape(s, s->lfo_phase >> 24);
    lfo = s->lfo_val;
    oct = dub_mul15(lfo, (int32_t)s->depth * 516);
    inc = dub_mul15u(s->phase_inc, (uint32_t)dub_exp2(oct));
    s->phase += inc;
    s->lfo_phase += s->lfo_inc;
    lev = (int32_t)s->level_cur;
    {
        int32_t tgt = (int32_t)s->level;
        lev += tgt > lev ? 1 : tgt < lev ? -1 : 0;
        s->level_cur = (uint16_t)lev;
    }
    if (!s->active && lev == 0)
        return 0;
    return dub_sir_osc(s, s->phase) * lev * 4;   /* *4, not <<2: the product may be negative;
                                                  * the same hot x2 scale sloop's siren had */
}

/* ---------------------------------------------------- the DUB-BOX master bus --- */
/* The master chain the audio callback runs, between the USB+BT mix and the
 * router's FX (EQ, limiter) and MASTER: the isolator, the DJ filter, then one
 * fused pass — the siren joined to the bus, the throws into the tape, the
 * tape's return. dub_on gates the whole module (the 8th black key's on/off, on
 * at boot); the siren sounds whenever its gate is down (the last black key),
 * dub_on or not. */
static uint8_t dub_on;
static dub_isolator_t dub_isolator;
static dub_siren_t dub_siren;

static void dub_init(void)
{
    dub_on = 1;                                      /* the module is on at boot: the 8th
                                                     * black key's LED lights (dub_ui.h) */
    dub_isolator_init(&dub_isolator);
    dub_isolator.enabled = 1;
    dub_siren_init(&dub_siren);
    dub_tape_init();
    dub_send_bt = 0;
    dub_send_usb_on = dub_send_bt_on = 1;
    dub_send_bt_g = 0;
    dub_sends_live = 0;
    dub_filter_set(64);                              /* the DJ filter back to FLAT, its
                                                      * glide and SVF histories cleared */
    dub_flt.amt = 0;
    dub_flt.hz = dub_flt.hz_t;
    dub_flt_ch[0].low = dub_flt_ch[0].band = 0;
    dub_flt_ch[1].low = dub_flt_ch[1].band = 0;
}
static void dub_master_process(int32_t *lr, uint32_t n)
{
    uint32_t i;
    uint8_t live = dub_sends_live && n <= APP_BLOCK;  /* the render left a fresh USB throw */
    uint8_t tape;
    dub_sends_live = 0;                          /* the flag is spent, whatever this call makes of it */
    if (!dub_on) {
        /* the module is off, but the siren may still sing */
        if (dub_siren.active || dub_siren.level_cur) {
            for (i = 0; i < n; i++) {
                int32_t y = dub_siren_tick(&dub_siren);
                lr[2 * i] += y;
                lr[2 * i + 1] += y;
            }
        }
        return;
    }
    tape = dub_tape_runs();
    if (tape) {
        /* the throws, before the isolator and the filter touch the bus: the USB
         * in as the render left it, the BT in as the mix minus that USB in. A
         * block without the render's capture throws silence. */
        if (live) {
            for (i = 0; i < n; i++) {
                int32_t m = (lr[2 * i] + lr[2 * i + 1]) >> 9;    /* the mix's mono, Q15 */
                int32_t bt;
                if (m > 32767)
                    m = 32767;
                else if (m < -32768)
                    m = -32768;
                bt = m - dub_throw_usb[i];
                if (bt > 32767)
                    bt = 32767;
                else if (bt < -32768)
                    bt = -32768;
                dub_throw_bt[i] = (int16_t)bt;
            }
        } else {
            for (i = 0; i < n; i++) {
                dub_throw_usb[i] = 0;
                dub_throw_bt[i] = 0;
            }
        }
    }
    dub_isolator_process(&dub_isolator, lr, n);
    dub_filter_process(lr, n);
    if (!tape) {
        /* the echo rests, but the siren may still sing */
        if (dub_siren.active || dub_siren.level_cur) {
            for (i = 0; i < n; i++) {
                int32_t y = dub_siren_tick(&dub_siren);
                lr[2 * i] += y;
                lr[2 * i + 1] += y;
            }
        }
        return;
    }
    dub_tape_block(n);
    for (i = 0; i < n; i++) {
        int32_t l = lr[2 * i], r = lr[2 * i + 1];
        int32_t y = dub_siren_tick(&dub_siren);
        int32_t tin = (dub_mul15(dub_throw_usb[i], dub_tape_send_g) << 9) +
                      (dub_mul15(dub_throw_bt[i], dub_send_bt_g) << 9) +
                      dub_mul15(y, dub_tape_sir_g);   /* the sources' throws (Q15 -> Q23), the siren's in */
        int32_t wet = dub_tape_step(tin);
        lr[2 * i] += y + wet;
        lr[2 * i + 1] += y + wet;
    }
}
#endif
