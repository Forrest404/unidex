# Generates the three 200x200 1-bit test badges in data/badges/. Needs Pillow; uses macOS Helvetica.
from PIL import Image, ImageDraw, ImageFont

H = '/System/Library/Fonts/Helvetica.ttc'
def font(size, bold=False): return ImageFont.truetype(H, size, index=1 if bold else 0)
def new(bg=1):
    im = Image.new('1', (200, 200), bg); d = ImageDraw.Draw(im); d.fontmode = '1'; return im, d

im, d = new()
d.rectangle((0, 0, 199, 62), fill=0)
d.text((100, 30), 'HELLO', font=font(34, True), fill=1, anchor='mm')
d.text((100, 54), 'my name is', font=font(14), fill=1, anchor='mm')
d.text((100, 124), 'Forrest', font=font(40, True), fill=0, anchor='mm')
d.rectangle((0, 186, 199, 199), fill=0)
im.save('data/badges/01-hello.bmp')

im, d = new()
S, G = 36, 10; mx, my = (200 - 2*S - G) // 2, 36
for (cx, cy, fill) in [(0, 0, False), (1, 0, False), (0, 1, False), (1, 1, True)]:
    x, y = mx + cx * (S + G), my + cy * (S + G)
    if fill: d.rectangle((x, y, x + S - 1, y + S - 1), fill=0)
    else: d.rectangle((x, y, x + S - 1, y + S - 1), outline=0, width=3)
d.text((100, 160), 'unidex', font=font(30), fill=0, anchor='mm')
im.save('data/badges/02-unidex.bmp')

im, d = new(0)
d.text((100, 82), 'ask me about', font=font(22), fill=1, anchor='mm')
d.text((100, 122), 'e-ink', font=font(44, True), fill=1, anchor='mm')
im.save('data/badges/03-ask.bmp')
