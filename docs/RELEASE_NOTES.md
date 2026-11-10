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
