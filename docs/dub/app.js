// made by tools/build.py: the emulator's parts (fm1.toml emu) and the app's own part of the page
import { compose } from "./parts/parts.js";
import * as data from "./parts/data.js";
import * as midi from "./parts/midi.js";
import * as reboot from "./parts/reboot.js";
import * as image from "./parts/image.js";
const own = {};
export const { attach, message } = compose({ data, midi, reboot, image }, own, { id: "dub-box", name: "DUB-BOX", backupExt: "bak" });
