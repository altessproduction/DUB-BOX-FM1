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
