/* SPDX-License-Identifier: GPL-3.0-only
 * DUB-BOX's DUB screen: the four pages DUB, SIREN, DELAY and ISO, sloop's dub
 * pages on the router's controls. OCT- enters and walks the pages, HOME leaves.
 * K1..K3 (and ALGO where a page has a fourth) rule the page's values, SEL does
 * the page's switch (the module on DUB, the siren's wave on SIREN), the mixer's
 * mute keys kill the ISO bands, REC (or SEL held) sounds the siren at LEVEL.
 * Included by app.c after fx/ui.h: cv_*, the fonts and the FX helpers exist. */
#define DUB_UI_ROW_Y0 44
#define DUB_UI_ROW_DY 42
static uint8_t gateway_dub_page;
enum { DUB_PG_MAIN = 0, DUB_PG_SIREN, DUB_PG_DELAY, DUB_PG_ISO, DUB_PG_N };
static const char *const gateway_dub_page_names[DUB_PG_N] = {"DUB", "SIREN", "DELAY", "ISOLATOR"};
static uint8_t gateway_dub_sir_lvl = 88;           /* the SIREN page's LEVEL, what REC sounds at */
static uint8_t gateway_dub_sir_held;               /* REC / held SEL: the siren sounds */

static void dub_tape_hold_wanted(void);
static void dub_changed(void)
{
    gateway_settings_changed();
    gateway_page_redraw = 1;
    dub_tape_hold_wanted();
}

/* the tape's storage lives in the work area, not .bss: 64 KiB there would eat
 * the room the waveform/spectrum captures need. It is held while the echo is
 * in use; the display capture forces it out (gateway_acquire_display) and it
 * comes back when the view is gone. Foreground only, never in the callback. */
static void *gateway_dub_tape_block;
static uint8_t dub_tape_wanted(void)
{
    return dub_on && (dub_tape_lvl || dub_tape_fb || dub_tape_send || dub_siren_send);
}
static void dub_tape_hold(int want)
{
    if (want && !gateway_dub_tape_block) {
        void *p = plat_work_alloc(dub_tape_bytes());
        if (p) {
            gateway_dub_tape_block = p;
            dub_tape_attach((int16_t *)p);
        }
    } else if (!want && gateway_dub_tape_block) {
        dub_tape_attach(0);
        plat_work_free(gateway_dub_tape_block);
        gateway_dub_tape_block = 0;
    }
}
static void dub_tape_hold_wanted(void) { dub_tape_hold(dub_tape_wanted()); }

/* the value rows: label left, value right, a bar under, one row a knob */
static void dub_ui_row(unsigned row, const char *label, const char *value, uint16_t color,
                       unsigned v, unsigned vmax)
{
    unsigned y = DUB_UI_ROW_Y0 + row * DUB_UI_ROW_DY;
    cv_text(6, y, &FONT_T, label, C_GRAY);
    cv_text(234 - text_w(&FONT_L, value), y - 4, &FONT_L, value, color);
    if (vmax) {
        unsigned w = (226u - 6u) * v / vmax;
        cv_rect(6, y + 24, 220, 5, RGB(22, 26, 24));
        cv_rect(6, y + 24, w, 5, color);
    }
}
static char *dub_ui_db(char text[12], int db)      /* "-12" .. "+12", 0 plain */
{
    unsigned n = 0, v = (db < 0 ? -db : db);
    if (db > 0) text[n++] = '+';
    if (db < 0) text[n++] = '-';
    if (v >= 10) { text[n++] = (char)('0' + v / 10); text[n++] = (char)('0' + v % 10); }
    else text[n++] = (char)('0' + v);
    text[n] = 0;
    return text + n;
}
static char *dub_ui_val_db(char *t, int db, const char *s)
{
    t = dub_ui_db(t, db);
    while (*s)
        *t++ = *s++;
    *t = 0;
    return t;
}
static char *dub_ui_hz(char text[12], uint32_t hz)  /* "400" or "3.8K", sloop's way */
{
    if (hz >= 1000) {
        unsigned t = (unsigned)((hz + 50) / 100), n = 0;
        if (t / 10 >= 10) {
            text[n++] = (char)('0' + (t / 10) / 10);
            text[n++] = (char)('0' + (t / 10) % 10);
        } else
            text[n++] = (char)('0' + t / 10);
        text[n++] = '.';
        text[n++] = (char)('0' + t % 10);
        text[n++] = 'K';
        text[n] = 0;
    } else {
        unsigned v = hz, n = 0, k = 0, d[4];
        do { d[k++] = v % 10u; v /= 10u; } while (v);
        while (k) text[n++] = (char)('0' + d[--k]);
        text[n] = 0;
    }
    return text;
}
static char *dub_ui_num(char text[12], unsigned v)   /* the number written, the cursor back (dub_ui_put's) */
{
    unsigned n = 0, k = 0, d[5];
    do { d[k++] = v % 10u; v /= 10u; } while (v);
    while (k) text[n++] = (char)('0' + d[--k]);
    text[n] = 0;
    return text + n;
}
static char *dub_ui_val(char *t, unsigned v, const char *s)   /* the number, then the unit after it */
{
    t = dub_ui_num(t, v);
    while (*s)
        *t++ = *s++;
    *t = 0;
    return t;
}
static char *dub_ui_put(char *t, const char *s)   /* append; no libc strcat here */
{
    while (*s)
        *t++ = *s++;
    *t = 0;
    return t;
}

static void gateway_dub_draw(void)
{
    char text[12];
    unsigned page = gateway_dub_page;
    /* One band a pass, the page's whole drawing inside (fx/gui.h's pattern):
     * every cv_ call takes the screen's y, cv_oy maps it into the band, and
     * the canvas clip drops what other bands hold. The canvas is 16 rows —
     * drawing the rows outside a band's strip drew nothing at all. */
    for (unsigned band = 18; band < 240; band += APP_CANVAS_PX / 240u) {
        unsigned h = 240 - band; if (h > APP_CANVAS_PX / 240u) h = APP_CANVAS_PX / 240u;
        cv_begin(240, h, C_BLACK); cv_oy = -(int)band;
        cv_text(6, 22, &FONT_XS, gateway_dub_page_names[page], RGB(96, 200, 255));
        const char *state = "";
        if (page == DUB_PG_MAIN) state = dub_on ? "ON" : "OFF";
        if (page == DUB_PG_SIREN) state = gateway_dub_sir_held ? "SOUND" : "";
        if (page == DUB_PG_DELAY) state = dub_tape_lvl ? "" : "MUTED";
        if (*state)
            cv_text(234 - text_w(&FONT_XS, state), 22, &FONT_XS, state,
                (page == DUB_PG_MAIN && !dub_on) || (page == DUB_PG_DELAY && !dub_tape_lvl) ? RGB(255, 90, 60) : RGB(40, 220, 90));
        cv_rect(0, 39, 240, 1, RGB(28, 31, 35));
        if (page == DUB_PG_MAIN) {
            int32_t v = dub_filter_get();
            if (v == 64) {
                dub_ui_put(text, "FLAT");
            } else {
                text[0] = v < 64 ? 'L' : 'H'; text[1] = 'P'; text[2] = ' ';
                dub_ui_hz(text + 3, dub_filter_hz(v));
            }
            dub_ui_row(0, "K1 FILTER", text, v == 64 ? C_GRAY : RGB(255, 212, 40),
                       v == 64 ? 0u : (unsigned)(v < 64 ? 64 - v : v - 64), 64u);
            dub_ui_num(text, dub_isolator.mix);
            dub_ui_row(1, "K2 MIX", text, C_WHITE, dub_isolator.mix, 127u);
            if (dub_tape_send) dub_ui_num(text, dub_tape_send); else dub_ui_put(text, "DRY");
            dub_ui_row(2, "K3 SEND", text, dub_tape_send ? RGB(75, 210, 150) : C_GRAY, dub_tape_send, 127u);
            cv_text(6, 218, &FONT_XS, "SEL ON/OFF", C_GRAY);
        } else if (page == DUB_PG_SIREN) {
            dub_ui_val(text, dub_siren.freq_hz, " HZ");
            dub_ui_row(0, "K1 PITCH", text, RGB(255, 140, 25), dub_siren.freq_hz - 220u, 980u);
            dub_ui_val(text, dub_siren_rate_hz(dub_siren.rate), " HZ");
            dub_ui_row(1, "K2 RATE", text, RGB(255, 140, 25), dub_siren_rate_hz(dub_siren.rate) - 1u, 15u);
            dub_ui_num(text, gateway_dub_sir_lvl);
            dub_ui_row(2, "K3 LEVEL", text, gateway_dub_sir_lvl ? RGB(255, 140, 25) : C_GRAY,
                       gateway_dub_sir_lvl, 127u);
            cv_text(6, 170 + 22, &FONT_XS, "ALGO", C_GRAY);
            cv_text(90, 168, &FONT_L, DUB_SIR_TYPE_NAMES[dub_siren.type], C_WHITE);
            cv_text(6, 218, &FONT_XS, "SEL WAVE   REC SOUNDS", C_GRAY);
        } else if (page == DUB_PG_DELAY) {
            dub_ui_val(text, dub_tape_ms, " MS");
            dub_ui_row(0, "K1 TIME", text, RGB(75, 155, 255), dub_tape_ms - 40u, DUB_TAPE_MS_MAX - 40u);
            dub_ui_num(text, dub_tape_fb);
            dub_ui_row(1, "K2 FEEDBK", text, C_WHITE, dub_tape_fb, 127u);
            dub_ui_num(text, dub_tape_lvl);
            dub_ui_row(2, "K3 LEVEL", text, dub_tape_lvl ? RGB(75, 155, 255) : RGB(255, 90, 60),
                       dub_tape_lvl, 127u);
            cv_text(6, 170 + 22, &FONT_XS, "ALGO", C_GRAY);
            dub_ui_num(text, dub_tape_tone);
            cv_text(90, 168, &FONT_L, text, C_WHITE);
            cv_text(6, 218, &FONT_XS, "HOME BACK", C_GRAY);
        } else {
            static const char *const band_names[DUB_BANDS] = {"K1 SUB", "K2 BASS", "K3 MIDS", "A4 TOPS"};
            for (unsigned b = 0; b < DUB_BANDS; b++) {
                const dub_band_param_t *p = &dub_isolator.p[b];
                if (p->mute) {
                    dub_ui_row(b, band_names[b], "KILL", RGB(255, 48, 48), 0, 0);
                } else {
                    dub_ui_val_db(text, p->gain_db, " DB");
                    dub_ui_row(b, band_names[b], text, p->gain_db ? C_WHITE : C_GRAY,
                               (unsigned)(p->gain_db + 24), 36u);
                }
            }
            cv_text(6, 218, &FONT_XS, "KEYS KILL   SEL LIFTS", C_GRAY);
        }
        cv_oy = 0; cv_blit(0, band);
    }
}
/* the page's knobs: enc is EN_*, delta the detents */
static void gateway_dub_turn(unsigned enc, int delta)
{
    unsigned page = gateway_dub_page;
    if (page == DUB_PG_MAIN) {
        if (enc == EN_K1) { dub_filter_set((int16_t)(dub_filter_get() + delta)); dub_changed(); }
        else if (enc == EN_K2) {
            int v = (int)dub_isolator.mix + delta;
            dub_isolator.mix = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
            dub_changed();
        } else if (enc == EN_K3) {
            int v = (int)dub_tape_send + delta;
            dub_tape_set_send((uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v));
            dub_changed();
        }
    } else if (page == DUB_PG_SIREN) {
        int step = gateway_fx_step(enc, delta);
        if (enc == EN_K1) { dub_siren_set_freq(&dub_siren, (uint16_t)(dub_siren.freq_hz + step * delta * 2)); dub_changed(); }
        else if (enc == EN_K2) { dub_siren_set_rate(&dub_siren, (uint8_t)(dub_siren.rate + step * delta)); dub_changed(); }
        else if (enc == EN_K3) {
            int v = (int)gateway_dub_sir_lvl + delta;
            gateway_dub_sir_lvl = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
            if (gateway_dub_sir_held) dub_siren_trigger(&dub_siren, gateway_dub_sir_lvl);
            dub_changed();
        } else if (enc == EN_ALGO) {
            dub_siren_set_type(&dub_siren, (uint8_t)((dub_siren.type + delta % DUB_SIR_TYPES + DUB_SIR_TYPES) % DUB_SIR_TYPES));
            dub_changed();
        }
    } else if (page == DUB_PG_DELAY) {
        int step = gateway_fx_step(enc, delta);
        if (enc == EN_K1) { dub_tape_set_ms((uint16_t)(dub_tape_ms + step * delta * 5)); dub_changed(); }
        else if (enc == EN_K2) { dub_tape_set_fb((uint8_t)(dub_tape_fb + step * delta)); dub_changed(); }
        else if (enc == EN_K3) { dub_tape_set_lvl((uint8_t)(dub_tape_lvl + step * delta)); dub_changed(); }
        else if (enc == EN_ALGO) { dub_tape_set_tone((uint8_t)(dub_tape_tone + step * delta)); dub_changed(); }
    } else {
        if (enc == EN_K1 || enc == EN_K2 || enc == EN_K3 || enc == EN_ALGO) {
            unsigned band = enc == EN_K1 ? DUB_SUB : enc == EN_K2 ? DUB_BASS : enc == EN_K3 ? DUB_MIDS : DUB_TOPS;
            dub_band_param_t p = dub_isolator.p[band];
            if (!p.mute) {
                int g = p.gain_db + delta;
                p.gain_db = (int8_t)(g < -24 ? -24 : g > 12 ? 12 : g);
                dub_isolator_set_band(&dub_isolator, band, p);
                dub_changed();
            }
        }
    }
}
/* the page's switch: SEL on DUB (module on/off) and SIREN (wave) */
static void gateway_dub_sel(void)
{
    if (gateway_dub_page == DUB_PG_MAIN) {
        dub_on = !dub_on;
        gateway_lights();
    } else if (gateway_dub_page == DUB_PG_SIREN) {
        dub_siren_set_wave(&dub_siren, (uint8_t)((dub_siren.wave + 1u) % DUB_SIW_N));
    } else if (gateway_dub_page == DUB_PG_ISO) {
        /* SEL lifts every kill: the full band comes back */
        for (unsigned b = 0; b < DUB_BANDS; b++) {
            dub_band_param_t p = dub_isolator.p[b];
            if (p.mute) {
                p.mute = 0;
                dub_isolator_set_band(&dub_isolator, b, p);
            }
        }
        gateway_lights();
    }
    dub_changed();
}
static void gateway_dub_kill(unsigned band)
{
    dub_band_param_t p = dub_isolator.p[band];
    p.mute = (uint8_t)!p.mute;
    dub_isolator_set_band(&dub_isolator, band, p);
    dub_changed();
}
static void gateway_dub_siren_hold(int down)
{
    if (down) {
        if (!gateway_dub_sir_lvl)
            gateway_dub_sir_lvl = 88u;
        dub_siren_trigger(&dub_siren, gateway_dub_sir_lvl);
        gateway_dub_sir_held = 1;
    } else {
        dub_siren.active = 0;
        gateway_dub_sir_held = 0;
    }
    gateway_lights();
    gateway_page_redraw = 1;
}
