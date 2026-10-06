// The Pet designer on the Tools page: the same parts and rules as the device (src/apps/pet/pet_logic.h).
import { LAYERS, EYES_CLOSED, DEFAULT_LOOK } from './pet-parts.js';

export { LAYERS, DEFAULT_LOOK };
export const SIZE = 32;

// A look is one part number per layer; saved as one number, 4 bits a layer, body in the lowest bits.
export const pack = look => look.reduce((v, p, i) => v + (p & 15) * 16 ** i, 0);
export const unpack = v => LAYERS.map((l, i) => {
  const p = Math.floor(v / 16 ** i) % 16;
  return p < l.parts.length ? p : DEFAULT_LOOK[i];
});
export const randomLook = () => LAYERS.map(l => Math.floor(Math.random() * l.parts.length));

// Draws the look on a canvas, layer by layer, at `scale` pixels per pixel, on white.
export function drawLook(canvas, look, { scale = 6, eyesClosed = false } = {}) {
  canvas.width = canvas.height = SIZE * scale;
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = '#fff';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  LAYERS.forEach((layer, i) => {
    const part = i === 1 && eyesClosed ? EYES_CLOSED : layer.parts[look[i]];
    part.rows.forEach((row, y) => [...row].forEach((c, x) => {
      if (c === '.') return;
      ctx.fillStyle = c === '#' ? '#1a1a1a' : '#fff';
      ctx.fillRect(x * scale, y * scale, scale, scale);
    }));
  });
}

// Up to 12 printable characters, as the device accepts.
export const cleanName = s => s.replace(/[^\x20-\x7e]/g, '').trim().slice(0, 12);
