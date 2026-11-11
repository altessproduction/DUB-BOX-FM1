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

// OCT- again: the SIREN page. REC (button 11) is out of the siren's way — the gate is the last black key alone
emu.send({ type: "buttons", mask: 1 << 12 });
emu.run(120);
emu.send({ type: "buttons", mask: 0 });
emu.run(300);
const quiet = emu.run(200);
emu.send({ type: "buttons", mask: 1 << 11 });
const recPeak = emu.run(300);
emu.send({ type: "buttons", mask: 0 });
check(recPeak <= quiet * 1.5 + 1e-4, `REC no longer sounds the siren (peak ${recPeak.toFixed(4)}, ${quiet.toFixed(4)} before)`);

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

// the last black key (25) is the siren's gate, module off: held it sings dry,
// released it stops — the fall is over in a few ms, nothing rings on
emu.send({ type: "keys", mask: 1 << 25 });
const loudKey = emu.run(300);
emu.send({ type: "keys", mask: 0 });
emu.run(30);                                  // let the fall pass
const drop = emu.run(150);
check(loudKey > 0.001 && loudKey > quiet * 4, `the last black key sounds the siren (peak ${loudKey.toFixed(4)})`);
check(drop < loudKey / 4, `the siren stops when the key is released (drop ${drop.toFixed(4)})`);
const litSiren = emu.take("frame").filter((f) => f.keys !== undefined).pop();
check(!!litSiren && !(litSiren.keys & (1 << 25)) && !!(litSiren.dimKeys & (1 << 25)), "the siren key's LED rests dim after the release");
emu.send({ type: "keys", mask: 1 << 1 });
emu.run(300);
emu.send({ type: "keys", mask: 0 });
emu.run(300);
const litKill = emu.take("frame").filter((f) => f.keys !== undefined).pop();
check(!!litKill && !!(litKill.keys & (1 << 1)) && !(litKill.keys & (1 << 25)), "the first black key's LED lights while SUB is killed");
emu.send({ type: "keys", mask: 1 << 1 });   // lift the kill: the key back to dim
emu.run(300);
emu.send({ type: "keys", mask: 0 });
emu.run(300);
const litLift = emu.take("frame").filter((f) => f.keys !== undefined).pop();
check(!!litLift && !!(litLift.dimKeys & (1 << 1)), "the second press lifts the kill (the key back to dim)");

// the SIREN page's K4 (role 6) is the siren's SEND — its own throw into the tape.
// DRY first (the turn clamps at 0): the released siren stops clean, no tail; then
// the send up: whatever it threw keeps ringing, the tail is the tape's
emu.send({ type: "enc", role: 6, n: -120 });
emu.run(200);
emu.send({ type: "keys", mask: 1 << 25 });
const dryGate = emu.run(400);
emu.send({ type: "keys", mask: 0 });
emu.run(30);
const dryTail = emu.run(900);
check(dryGate > 0.001, `the siren sings on the SIREN page (peak ${dryGate.toFixed(4)})`);
check(dryTail < dryGate / 4, `SEND DRY: the released siren leaves no tail (tail ${dryTail.toFixed(4)})`);
emu.send({ type: "enc", role: 6, n: 40 });
emu.run(200);
emu.send({ type: "keys", mask: 1 << 25 });
emu.run(400);
emu.send({ type: "keys", mask: 0 });
emu.run(30);
const sendTail = emu.run(900);                // the repeats, from the tape time on
check(sendTail > 2e-4, `the siren's own SEND keeps a tail after the release (tail ${sendTail.toFixed(4)})`);

// the tape's tail through the DUB throw: the walk SIREN→SIREN 2→DELAY→ISO→DUB
// takes four OCT- presses. On DELAY, K4 (role 6) turns the tone — the page's
// fourth knob, sloop's K4 — then K3 (role 5) on DUB opens the throw and the
// released siren keeps ringing in the delay even though its own voice is gone
for (let p = 0; p < 2; p++) {
  emu.send({ type: "buttons", mask: 1 << 12 });
  emu.run(120);
  emu.send({ type: "buttons", mask: 0 });
  emu.run(120);
}
emu.run(200);
const fbDLY = emu.take("frame").filter((f) => f.fb).pop();
emu.send({ type: "enc", role: 6, n: -30 });
emu.run(300);
const fbTONE = emu.take("frame").filter((f) => f.fb).pop();
check(!!fbTONE && !!fbDLY && diff(fbDLY, fbTONE) > 20, `K4 turns the delay's tone on DELAY (${fbTONE ? diff(fbDLY, fbTONE) : 0} px change)`);
for (let p = 0; p < 2; p++) {
  emu.send({ type: "buttons", mask: 1 << 12 });
  emu.run(120);
  emu.send({ type: "buttons", mask: 0 });
  emu.run(120);
}
emu.run(200);
emu.send({ type: "enc", role: 5, n: 40 });
emu.run(300);
emu.send({ type: "keys", mask: 1 << 25 });
const loudGate = emu.run(400);
emu.send({ type: "keys", mask: 0 });
emu.run(30);
const tail = emu.run(900);                    // the repeats, from the tape time on
check(loudGate > 0.001, `the gated siren sings with the module on (peak ${loudGate.toFixed(4)})`);
check(tail > 2e-4, `the tape keeps the siren's tail ringing after the release (tail ${tail.toFixed(4)})`);

// PLAY (button 10) mutes the master: the lights' PLAY bit and the sound stop
emu.run(100);                                // queue a frame (the checks above drained them)
const lit0 = emu.take("frame").slice(-1)[0];
emu.send({ type: "buttons", mask: 1 << 10 });
emu.run(150);
emu.send({ type: "buttons", mask: 0 });
emu.run(300);
const lit1 = emu.take("frame").filter((f) => f.buttons !== undefined).pop();
check(!!lit1 && lit0 && !!(lit1.buttons & (1 << 10)), "PLAY lights when the master is muted");

done();
