// Runs the emulator in Node: sends the firmware's own test commands and prints what the screen shows.
const path = require('path');
const factory = require(path.join(__dirname, '../out/unidex.js'));
let frames = [];
factory({
  onScreen(bytes, kind) { frames.push({ kind, buf: Buffer.from(bytes) }); },
  print: (t) => console.log("[out]", t), printErr: (t) => console.log("[err]", t),
}).then((m) => { M = m; run(); });
let M;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
function say(cmd) { M.ccall('emu_serial_in', null, ['string'], [cmd + '\n']); }
function out() { return M.ccall('emu_serial_out', 'string', [], []); }
async function ask(cmd, wait = 400) { say(cmd); await sleep(wait); return out().trim(); }
function ascii(buf) {
  let s = '';
  for (let y = 0; y < 200; y += 4) {
    for (let x = 0; x < 200; x += 2) {
      const bit = (yy, xx) => (buf[(yy * 200 + xx) >> 3] & (0x80 >> (xx & 7))) === 0;
      s += bit(y, x) || bit(y + 1, x) || bit(y + 2, x + 1) || bit(y + 3, x + 1) ? '#' : ' ';
    }
    s += '\n';
  }
  return s;
}
async function run() {
  await sleep(5000);
  console.log('STATE:', await ask('X STATE'));
  console.log('frames so far:', frames.length);
  if (frames.length) console.log(ascii(frames[frames.length - 1].buf));
  process.exit(0);
}
