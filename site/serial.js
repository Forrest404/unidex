// Talks to a unidex over USB with Web Serial (Chrome / Edge on desktop).
// The device's protocol (src/core/usbsync.cpp): "?" -> "unidex 1", T <unix> sets the clock,
// E <count> <crc32> + lines sends calendar events, L / B / D list and upload badges.

export const supported = 'serial' in navigator;

// zlib crc32, the same as the device's esp_rom_crc32_le.
export const crc32 = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return bytes => {
    let c = 0xFFFFFFFF;
    for (const b of bytes) c = t[(c ^ b) & 255] ^ (c >>> 8);
    return (c ^ 0xFFFFFFFF) >>> 0;
  };
})();

export const asleepHint = 'No device picked. Nothing listed? The board is asleep: press one of its buttons, then try again.';

// Asks the user to pick the board, opens it and says hello. Returns {send, expect, close}.
export async function connect() {
  const port = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a }] });
  await port.open({ baudRate: 115200 });
  // The ESP32-S3 resets if RTS is on while DTR is off, so clear RTS first, then DTR.
  await port.setSignals({ requestToSend: false });
  await port.setSignals({ dataTerminalReady: false });
  const writer = port.writable.getWriter(), reader = port.readable.getReader();
  const enc = new TextEncoder(), dec = new TextDecoder();
  let buf = '';
  const pump = (async () => {
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        buf += dec.decode(value, { stream: true });
      }
    } catch (e) { /* port closed */ }
  })();
  const send = text => writer.write(enc.encode(text + '\n'));
  // Next line starting with `prefix` (others are skipped), or null after `ms`. "ERR" throws.
  const expect = async (prefix, ms, onOther) => {
    const end = Date.now() + ms;
    while (Date.now() < end) {
      let nl;
      while ((nl = buf.indexOf('\n')) >= 0) {
        const line = buf.slice(0, nl).replace(/\r$/, '');
        buf = buf.slice(nl + 1);
        if (line.startsWith(prefix)) return line;
        if (line === 'ERR') throw new Error('The device rejected it.');
        if (onOther) onOther(line);
      }
      await new Promise(r => setTimeout(r, 10));
    }
    return null;
  };
  const close = async () => {
    await reader.cancel().catch(() => {});
    reader.releaseLock();
    writer.releaseLock();
    await pump;
    await port.close().catch(() => {});
  };
  let hello = null;
  for (let i = 0; i < 6 && !hello; i++) {  // it may still be busy redrawing after a wake
    await send('?');
    hello = await expect('unidex 1', 1000);
  }
  if (!hello) {
    await close();
    throw new Error('No reply. Is unidex installed? Press a button on the device to wake it, then try again.');
  }
  return { send, expect, close };
}
