// Writes the newest firmware (firmware/firmware.bin, the app only, at 0x10000) and never erases, so the settings
// (NVS) and the SD card stay. ESP Web Tools can't do this: it erases the whole device for firmware without Improv
// Serial. Used by the Install page's Update button and the Tools page's one-click sync.
import { ESPLoader, Transport } from 'https://cdn.jsdelivr.net/npm/esptool-js@0.6.0/+esm';

// The version the website offers ("v1.4"), from the release that built it; null if it can't be read.
export async function latestVersion() {
  try {
    const r = await fetch('firmware/update.json', { cache: 'no-store' });
    return r.ok ? (await r.json()).version : null;
  } catch (e) {
    return null;
  }
}

// `port` must be closed (esptool opens it). status(text) gets the steps and the percentage.
export async function updateFirmware(port, status) {
  const transport = new Transport(port);
  const loader = new ESPLoader({ transport, baudrate: 115200, enableTracing: false });  // 115200: no baud switch
  try {
    status('Connecting…');
    await loader.main();  // resets into the bootloader and starts the flasher
    status('Downloading…');
    const resp = await fetch('firmware/firmware.bin', { cache: 'no-store' });
    if (!resp.ok) throw new Error(`download failed (${resp.status})`);
    const data = new Uint8Array(await resp.arrayBuffer());
    await loader.writeFlash({
      fileArray: [{ data, address: 0x10000 }],
      flashSize: 'keep', flashMode: 'keep', flashFreq: 'keep', eraseAll: false, compress: true,
      reportProgress: (i, written, total) => status(`Updating… ${Math.floor(written * 100 / total)}%`),
    });
    status('Restarting…');
    await transport.setRTS(true);
    await new Promise(r => setTimeout(r, 100));
    await loader.after();
  } finally {
    await transport.disconnect().catch(() => {});
  }
}
