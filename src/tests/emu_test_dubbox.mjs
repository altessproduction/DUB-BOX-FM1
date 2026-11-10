// SPDX-License-Identifier: GPL-3.0-only
// DUB-BOX's emulator checks (src/tests/emu_harness.mjs): the real firmware in app.wasm — boot, the DUB
// pages' screens, the siren sounding, the master mute, the live USB playback path, the lights.
//   node src/tests/emu_test_dubbox.mjs build/gateway/emu
import { startEmu, check, done } from "./emu_harness.mjs";

const dir = process.argv[2] || "build/gateway/emu";
const emu = await startEmu(dir);
check(emu.take("ready").length === 1, "the worklet says ready after the load");
emu.run(700);
const frames = emu.take("frame");
const fb0 = frames.filter((f) => f.fb).pop();
check(frames.length >= 10, `frames reach the page (${frames.length} in 0.7 s)`);
check(!!fb0 && fb0.fb.some((v) => v !== 0), "the screen is drawn after boot");

// OCT- (button 12) enters the DUB screen: the screen's content changes
emu.send({ type: "buttons", mask: 1 << 12 });
emu.run(120);
emu.send({ type: "buttons", mask: 0 });
emu.run(500);
const fbDUB = emu.take("frame").filter((f) => f.fb).pop();
const diff = (a, b) => a.fb.filter((v, i) => v !== b.fb[i]).length;
check(!!fbDUB && diff(fb0, fbDUB) > 200, `OCT- opens the DUB screen (${fbDUB ? diff(fb0, fbDUB) : 0} px change)`);

// KNOB1 (role 3) sweeps the DJ filter: the screen changes again
emu.send({ type: "enc", role: 3, n: -30 });
emu.run(400);
const fbK1 = emu.take("frame").filter((f) => f.fb).pop();
check(!!fbK1 && diff(fbDUB, fbK1) > 30, `K1 sweeps the filter (${fbK1 ? diff(fbDUB, fbK1) : 0} px change)`);

// OCT- again: the SIREN page. REC (button 11) sounds the siren from any screen: audio out!
emu.send({ type: "buttons", mask: 1 << 12 });
emu.run(120);
emu.send({ type: "buttons", mask: 0 });
emu.run(300);
const quiet = emu.run(200);
emu.send({ type: "buttons", mask: 1 << 11 });
const loud = emu.run(300);
emu.send({ type: "buttons", mask: 0 });
check(loud > 0.001 && loud > quiet * 4, `the siren sounds (peak ${loud.toFixed(4)}, ${quiet.toFixed(4)} before)`);

// the live USB playback: the page's source into the emulated computer's port, through the router's mix
emu.send({ type: "uac", on: true });
const sr = 44100, n = 128;
const sine = new Float32Array(n).map((_, i) => Math.sin(2 * Math.PI * 220 * i / sr) * 0.8);
const inL = new Float32Array(n), inR = new Float32Array(n);
let peakUSB = 0;
for (let q = 0; q < 40; q++) {
  inL.set(sine); inR.set(sine);
  emu.proc.process([[inL, inR]], [emu.out]);
  for (const ch of emu.out) for (const v of ch) peakUSB = Math.max(peakUSB, Math.abs(v));
}
check(peakUSB > 0.001, `the page's source plays through the router (peak ${peakUSB.toFixed(4)})`);
emu.send({ type: "uac", on: false });

// PLAY (button 10) mutes the master: the lights' PLAY bit and the sound stop
const lit0 = emu.take("frame").slice(-1)[0];
emu.send({ type: "buttons", mask: 1 << 10 });
emu.run(150);
emu.send({ type: "buttons", mask: 0 });
emu.run(300);
const lit1 = emu.take("frame").filter((f) => f.buttons !== undefined).pop();
check(!!lit1 && lit0 && !!(lit1.buttons & (1 << 10)), "PLAY lights when the master is muted");

done();
