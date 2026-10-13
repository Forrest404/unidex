// A serial port to the emulator, on stdin and stdout, so tools/devshot.py can drive it like a device:
//   python3 tools/devshot.py run tools/walkthroughs/home.txt --emu
const path = require('path');
const readline = require('readline');
const createUnidex = require(path.join(__dirname, '../out/unidex.js'));
let M = null;
const pending = [];
createUnidex({
  onScreen() {},
  print: () => {},
  printErr: (t) => process.stderr.write(t + '\n'),
}).then((m) => {
  M = m;
  for (const l of pending) M.ccall('emu_serial_in', null, ['string'], [l + '\n']);
  setInterval(() => {
    const o = M.ccall('emu_serial_out', 'string', [], []);
    if (o) process.stdout.write(o);
  }, 10);
});
readline.createInterface({ input: process.stdin }).on('line', (l) => {
  if (M) M.ccall('emu_serial_in', null, ['string'], [l + '\n']);
  else pending.push(l);
}).on('close', () => process.exit(0));
