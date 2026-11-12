/* SPDX-License-Identifier: GPL-3.0-only
 * DUB-BOX's exports for its emulator's page (fm1-base's APP_WEB hook: emu_web.c includes
 * this at its end, where the app's statics already exist). The readback: the DUB screen's
 * state, packed for the web page's K1..K4 potentiometers — src/web/emu/worklet.js (the
 * app's fork) publishes it with each frame, and web/emu/index.html parses it. The pots
 * turn from the values the firmware really holds, whoever wrote them: the page's own
 * turns, the black keys, the boot's saved settings, a restart. Layout: 26 bytes,
 * little-endian, __attribute__((packed)) — the page's parser must match. The first 20
 * bytes keep the readback's first layout (the v1 page parses them the same). */
typedef struct __attribute__((packed)) {
    uint8_t page;                       /* DUB_PG_* */
    uint8_t type, wave;                 /* SIREN's K1/K2 lists, the item's index */
    uint16_t freq;                      /* SIREN K3 PITCH, 220..1200 Hz */
    uint8_t send;                       /* SIREN K4 SEND, 0..127 */
    uint8_t rate, depth, sym, sirlvl;   /* SIREN 2's K1..K4, 0..127 */
    uint16_t ms;                        /* DELAY K1 TIME, 40..740 ms */
    uint8_t fb, lvl, tone;              /* DELAY's K2/K3/K4, 0..127 */
    int8_t gain[4];                     /* ISO's K1..K4, -24..+12 dB */
    uint8_t mute;                       /* ISO: bit b set when band b is killed */
    uint8_t filter, mix;                /* DUB's K1/K2, 0..127 (64 filter = FLAT) */
    uint8_t usend, bsend;               /* DUB's K3/K4, the USB in and BT in throws, 0..127 */
    uint8_t state;                      /* bit 0 the module on, bit 1 the USB send armed,
                                         * bit 2 the BT send armed */
    uint8_t screen;                     /* gateway_screen: the pots live on the DUB screen */
} gateway_dub_wire_t;

static gateway_dub_wire_t gateway_dub_wire;
__attribute__((used, visibility("default"))) const uint8_t *web_dub(void)
{
    gateway_dub_wire_t *w = &gateway_dub_wire;
    w->page = gateway_dub_page;
    w->type = dub_siren.type; w->wave = dub_siren.wave;
    w->freq = dub_siren.freq_hz;
    w->send = dub_siren.send;
    w->rate = dub_siren.rate; w->depth = dub_siren.depth; w->sym = dub_siren.sym;
    w->sirlvl = gateway_dub_sir_lvl;
    w->ms = dub_tape_ms; w->fb = dub_tape_fb; w->lvl = dub_tape_lvl; w->tone = dub_tape_tone;
    for (unsigned b = 0; b < DUB_BANDS; b++)
        w->gain[b] = dub_isolator.p[b].gain_db;
    w->mute = (uint8_t)((dub_isolator.p[0].mute ? 1u : 0u) | (dub_isolator.p[1].mute ? 2u : 0u) |
                        (dub_isolator.p[2].mute ? 4u : 0u) | (dub_isolator.p[3].mute ? 8u : 0u));
    w->filter = (uint8_t)dub_filter_get();
    w->mix = dub_isolator.mix;
    w->usend = dub_tape_send; w->bsend = dub_send_bt;
    w->state = (uint8_t)((dub_on ? 1u : 0u) | (dub_send_usb_on ? 2u : 0u) | (dub_send_bt_on ? 4u : 0u));
    w->screen = gateway_screen;
    return (const uint8_t *)w;
}

__attribute__((used, visibility("default"))) uint32_t web_dbg_page_addr(void) { return (uint32_t)(uintptr_t)&gateway_dub_page; }
