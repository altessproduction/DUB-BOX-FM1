# DUB-BOX 1.0.7

The sources' own sends, and the dub module on the eighth black key:

- **The DUB page's K1–K4 are pots**, the same 300-degree course (twenty
  detents, one share a detent however fast) as the other DUB pages: the filter's
  whole lowpass–flat–highpass course, the isolator's mix, and the two new
  throws.
- **K3 is the USB in's send into the tape echo, K4 the BT in's** — two throws
  the tape reads separately. The render hands the USB in bus over before the
  Bluetooth mix joins it; the BT throw is the mix minus that USB in, taken
  before the isolator and the filter touch the bus. A send at 0 is DRY; both
  send **levels are 0 at boot**.
- **The eighth black key (A♯4, the GLO print) turns the DUB module on and
  off** — its LED is lit while the module is on, and **the module is on at
  boot**. The old switch is gone: SEL on the DUB page does nothing now (SEL
  keeps the siren's wave on SIREN and the kills' lift on ISO).
- **The ninth (C♯5, the MONO print) and tenth (D♯5) black keys arm the USB in
  and the BT in sends**, each LED lit while its send is armed — **both arms are
  on at boot**, so the send delay is active with its send levels at 0.
- The tape's room is held only while a send is connected or the siren sings
  (its tail keeps the room a few seconds past the gate) — at rest the work area
  stays whole for the waveform and spectrum captures. The status bar's DUB
  badge is gone: with the module on at boot it would have eaten the CPU and
  battery text for good.
- Settings carry **v19** (the BT throw's level and the two arms appended to the
  v18 record; v17/v18 records load unchanged, their sends defaulting to armed
  at 0). The emulator's page parses the 26-byte readback and the DUB page's
  caps sit at the firmware's real values like the other pages'; the wasm
  rebuilt (emcc 7.0.0).

Package identity `FM-1_2345`, `dub-box-1.0.7.fwsc` (613 178 B,
sha256 e56e676f…6e0f0).

# DUB-BOX 1.0.6

The firmware-hub submission release — the files the [firmware
hub](https://thefirmwarehub.com/) reads are in place:

- **fmhub.json** completed: the card's description, the author, the kind
  (`other` — a dub station, not a synth or a groovebox), the emulator as the
  hub's **Play** link, the card image (`assets/github/dubbox.png`, 240×240,
  43 KB) and the card colour (DUB-BOX orange, `#FF621A`).
- **The package identity is the firmware's own hub ID, plain `FM-1_2345`** —
  unique on the hub (checked against the catalog; sloop-DUB keeps
  `FM-1_234`), no version digits appended; the Git tag names the release
  (1.0.6).
- **The v1.0.5 tag's CI failure fixed**: fm1.toml carried a C-style comment
  (`/* … */`) on the `web_cflags` line — TOML only takes `#`. The 1.0.5
  package itself was sound (built and published from the same source); only
  the workflow's parse of the config was broken.
- The browser emulator's wasm rebuilt with the 1.0.6 label (the same code,
  emcc 7.0.0): the **Play** page on the hub runs the real 1.0.6 firmware.

Package identity `FM-1_2345`, `dub-box-1.0.6.fwsc` (613 178 B,
sha256 4627fb5e…a0e447).

# DUB-BOX 1.0.5

The DUB pages' knobs feel like the pots they are named after — sloop 2.9.8's
potentiometer feel, on the device and in the emulator:

- **The continuous K1–K4 of SIREN, SIREN 2, DELAY and ISO are pots**: a detent
  sweeps its share of the range, the whole course runs in some 300 degrees
  (20 detents; an FM-1 turn is 24 detents), one share a detent however fast —
  no acceleration, a turn never outruns the pointer. The siren's pitch crosses
  220–1200 Hz in 20 detents, the delay's time 40–740 ms in 20, the ISO gains
  −24…+12 dB in 18. TYPE and WAVE stay lists, a step a detent (five names over
  300 degrees would crawl).
- **The emulator's K1–K4 match**: the caps show the 300° travel with the part
  of the course already covered in the knob's colour, the pointer sits at the
  value the firmware really holds — a new readback (`web_dub`, app.c's APP_WEB
  export, published with each frame) keeps them honest through SEL turns, the
  saved settings at boot and page walks. Turn them by dragging up/down, by a
  circular turn around the knob, or a detent a wheel notch; the DUB page and
  the lists stay free-spinning encoders.
- The RAM-code vouch list follows the newer JieLi clang's inlining (the vouched
  uECC entries appear as their internals; the gone symbols are marked with `?`).

Package identity `FM-1_235`, `dub-box-1.0.5.fwsc` (613 534 B,
sha256 c61824c2…91d0d).

# DUB-BOX 1.0.4

The dub pages carry sloop-DUB's full settings, on the knobs they sit on there:

- **The SIREN page is sloop's SIR page**: K1 the LFO type (WAIL, HILO, RAMP,
  SWEEP, SPACE), K2 the wave (SQR, PLS, SAW, TRI, SIN — SEL still turns it),
  K3 the pitch 220–1200 Hz, **K4 the send** — the siren's own throw into the
  tape echo, 0 DRY. The send defaults to 48, sloop's dubby echo: the siren
  feeds the tape from the first boot, and what it threw keeps ringing after
  the gate opens — the tail is the tape's.
- **A second siren page, sloop's SR2**: K1 the rate 1–16 Hz, K2 the depth
  (the pitch sweep's amplitude, two octaves at 127), K3 the LFO's symmetry,
  K4 the level the last black key sounds at. DEPTH joins the persisted
  settings (v18 appends it; v17 records keep their saved values and the
  fresh-boot depth).
- **The fourth setting lives on K4 everywhere** — the delay's tone and the
  isolator's TOPS band move from the ALGORITHM knob to K4, sloop's knob order
  (KNOB 1..4 rule the page's settings). ALGORITHM is the router's FX knob
  again.
- The knob turns clamp in int before the cast: no more wrap-to-max jump when
  a value at its floor turns down.
- The browser emulator runs the same firmware; its manual documents the five
  pages. The host suites cover the siren's own send (DRY leaves no tail, the
  send does) and the delay's K4.

Package identity `FM-1_235`, `dub-box-1.0.4.fwsc` (size noted on the release).

# DUB-BOX 1.0.3

The siren answers to one key and behaves like a held note:

- **The last black key (F♯5) is the siren's only trigger** — a gate, not a
  switch: hold it and the siren sings at its LEVEL, release it and it stops.
  REC and a held SEL no longer sound it (SEL on the DUB screen is back to
  being just the page's switch, REC is out of the siren's way).
- **The release actually stops the sound.** The old release cleared the
  siren's active flag but left its level hot, so the note rang forever; the
  gate now drops the level back over a few milliseconds (no click) and the
  voice sleeps.
- **The delay tail stays.** Whatever the siren threw into the tape echo (the
  DUB page's K3 throw, or its own send) keeps ringing after the release — the
  tail is the tape's, not the key's, and it dies away with the repeats.
- The siren key's LED is lit while the key is held and rests dim after.
- The browser emulator's page runs the same firmware and documents the gate;
  the host suites cover the release (a short fall, then silence) and REC's
  new neutrality.

Package identity `FM-1_235`, `dub-box-1.0.3.fwsc` (size noted on the release).

# DUB-BOX 1.0.2

The keyboard joins the performance — the black keys drive the dub:

- **The last black key sounds the siren**, from any screen, like REC: hold it
  and it sings, release it and it dies away. REC and a held SEL (on the DUB
  page) keep working; the three share one hold, and the siren stops when all
  are up. The key's LED is lit while the siren sounds.
- **The first four black keys (F♯3 G♯3 A♯3 C♯4, the OP1–OP4 prints) kill the
  ISO bands** — SUB, BASS, MIDS, TOPS, one toggle each, from any screen (the
  kills used to sit on the first four white keys and only on the ISO page).
  All four together is the total kill, SEL on the ISO page lifts every kill,
  and a kill wakes the module (DUB ON) if it was off. The keys' LEDs show the
  kills on every screen; the ISO page's hint now reads "BLACK KEYS KILL".
- The mixer's white mute/solo keys (F3–B3 / F4–B4) are unchanged.
- The browser emulator's manual documents the keys; the host suites (445
  checks) exercise the keys from the HOME screen.

Package identity `FM-1_235`, `dub-box-1.0.2.fwsc` (size noted on the release).

# DUB-BOX 1.0.1

Two drawing fixes on the DUB screen (they hit the device the same way they hit
the browser), and the web emulator now runs the real firmware:

- **The DUB pages drew nothing** (the status bar only): `dub_ui.h`'s header
  band loop subtracted the band from its own y (the platform's `cv_` band
  pattern takes the screen's y and lets `cv_oy` map it), and the value rows
  drew after the loop with the canvas still 16 rows tall — everything landed
  outside it. The page now draws the whole DUB screen inside the band loop,
  the way `fx/gui.h` does.
- **The values with a unit lost their digits** ("HZ", "MS", "DB" with no
  number): `dub_ui_num`/`dub_ui_db` return the buffer's start, so
  `dub_ui_put(dub_ui_num(..), " HZ")` overwrote the number with the unit.
  They now return the write cursor (as `dub_ui_put` always has), and
  `dub_ui_val`/`dub_ui_val_db` write the number and the unit in one call.
- **The browser emulator runs the firmware itself**: `tools/build.py --web`
  (fm1-base's target, ported) compiles the whole app to `app.wasm` — screen,
  menus, sound, saved settings are the firmware's own code, with the FM-1
  faceplate after sloop-DUB's emulator. The page's source (built-in dub beat,
  microphone, audio file) plays through the emulated USB port via a new
  `FM1B_EMU_UAC` live path in `host/plat_host.c`'s `usb_play_take`. Emulator
  checks in `src/tests/emu_test_dubbox.mjs`; host suites pass (all current
  project suites, dub modules included).

Package identity `FM-1_235`, `dub-box-1.0.1.fwsc` (613 178 B).

# DUB-BOX 1.0.0

The first release. The FM-1 audio router gains a dub station on top:

- The **DUB screen** — press OCT− from anywhere — with the four pages:
  - **DUB**: the DJ filter (bipolar, lowpass left, highpass right, FLAT at the
    center detent, the sweep glides), the module's mix, and the throw (SEND)
    into the tape echo.
  - **SIREN**: the dub siren from sloop-DUB — five LFO types (WAIL, HILO, RAMP,
    SWEEP, SPACE) × five waves (SQR, PLS, SAW, TRI, SIN), pitch 220–1200 Hz,
    rate 1–16 Hz, level. REC (or SEL held) sounds it from any screen.
  - **DELAY**: the tape echo — time 40–740 ms, feedback, level, tone. The time
    glides like a tape's sag, the head wanders (wow 0.7 Hz, flutter 6.3 Hz) and
    the repeats pass the damping and the soft clip, warming pass by pass.
  - **ISOLATOR**: four bands (SUB 90 / BASS 420 / MIDS 2600 / TOPS), gains
    −24…+12 dB, kills on the mixer's mute keys — all four is the total kill.
- The dub chain sits between the USB+BT mix and the master FX; at rest it is
  bit-transparent (host-tested), and the whole module switches off.
- The dub state persists: settings v17 carries it across boots, migrating the
  router's v16 records untouched.
- Everything the router does stays: USB 2in/2out, the Bluetooth receiver
  (SBC/AAC/aptX/aptX HD), mixer, EQ/limiter, meters, waveforms, spectrum.

Package identity `FM-1_235` (fixed across releases, in sloop-DUB's own-identity
scheme). Install with M-Flasher on the m-bootloader — see the install page or
docs/INSTALL.md. The dub DSP's browser port is validated bit-exact against the
firmware C; host suites pass (445 checks).

Engineering note: the dub modules cost ~6 KiB of SRAM-resident code, which this
build spends on the audio router's memory contract's aptX corner. SBC and AAC
keep the full guarantee (display capture + USB-OUT recording route + decoder);
for aptX/HD the corner degrades the router's own graceful way — the spectrum
view can then report DISPLAY MEMORY FULL (it keeps retrying) and the USB OUT
route can refuse with a notice. No crash, no audio damage, the dub chain at
rest is bit-transparent. See src/tests/cpu2_runtime_test.py.
