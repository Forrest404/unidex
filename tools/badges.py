# Makes the 1-bit BMPs in data/badges/. Needs Pillow; works from any folder.
#   python3 tools/badges.py                   rebuild data/badges/*.bmp from PNG exports sitting next to them
#   python3 tools/badges.py photo.jpg [...]   add any image(s) as new badges (next number, name from file)
# Options:
#   --crop                  fill the whole screen, cutting the edges (added images only; default fits inside)
#   --dither / --no-dither  force either; by default photos are dithered and line art gets clean edges
import re, subprocess, sys, tempfile
from pathlib import Path
from PIL import Image, ImageOps, UnidentifiedImageError

ROOT = Path(__file__).resolve().parent.parent
DST, SIZE, MAX_BADGES = ROOT / 'data/badges', 200, 32

def load(path):
    try:
        im = Image.open(path)
    except UnidentifiedImageError:  # e.g. iPhone HEIC: let macOS convert it
        tmp = Path(tempfile.mkdtemp()) / 'in.png'
        if subprocess.run(['sips', '-s', 'format', 'png', str(path), '--out', str(tmp)],
                          capture_output=True).returncode != 0:
            sys.exit(f"{path}: can't read this image")
        im = Image.open(tmp)
    im = ImageOps.exif_transpose(im).convert('RGBA')  # phone photos are often stored sideways
    white = Image.new('RGBA', im.size, 'white')
    white.alpha_composite(im)  # transparent areas become white, not black
    return white.convert('L')

def is_photo(im):
    # Line art is almost all black or white (only anti-aliased edges are grey); photos aren't.
    hist = im.histogram()
    return sum(hist[32:224]) > 0.15 * im.width * im.height

def to_1bit(im, dither):
    if dither is None: dither = is_photo(im)
    if dither: return ImageOps.autocontrast(im, cutoff=1).convert('1')
    return im.point(lambda v: 255 if v >= 128 else 0).convert('1', dither=Image.Dither.NONE)

def save(im, name, src):
    im.save(DST / f'{name}.bmp')
    note = '' if im.size == (SIZE, SIZE) else f'  ({im.width}x{im.height}, will be centred)'
    print(f'{src} -> data/badges/{name}.bmp{note}')

def rebuild(dither):
    sources = sorted(p for p in DST.iterdir() if p.suffix.lower() == '.png')
    if not sources: sys.exit('no PNGs in data/badges/ - export your artboards there first')
    made = set()
    for src in sources:
        name = re.sub(r'@\d+(\.\d+)?x$', '', src.stem)  # Export for Screens adds "@1x" etc.
        im = load(src)
        if im.width > SIZE or im.height > SIZE:
            im.thumbnail((SIZE, SIZE), Image.LANCZOS)  # e.g. a 2x export
        save(to_1bit(im, dither), name, src.name)
        made.add(name)
    for old in sorted(DST.glob('*.bmp')):
        if old.stem not in made: print(f'{old.name}: no PNG source, left as is')

def add(paths, crop, dither):
    existing = list(DST.glob('*.bmp'))
    numbers = [int(m.group(1)) for p in existing if (m := re.match(r'(\d+)-', p.name))]
    n = max(numbers, default=0)
    for path in map(Path, paths):
        if not path.is_file(): sys.exit(f'{path}: no such file')
        im = load(path)
        if crop: im = ImageOps.fit(im, (SIZE, SIZE), Image.LANCZOS)
        else: im = ImageOps.contain(im, (SIZE, SIZE), Image.LANCZOS)  # scales small images up too
        n += 1
        slug = re.sub(r'[^a-z0-9]+', '-', path.stem.lower()).strip('-')[:24] or 'badge'
        save(to_1bit(im, dither), f'{n:02d}-{slug}', path.name)
    if len(existing) + len(paths) > MAX_BADGES:
        print(f'note: the badge app shows only the first {MAX_BADGES} badges')
    print('upload with: pio run -t uploadfs')

args = [a for a in sys.argv[1:] if not a.startswith('--')]
flags = set(sys.argv[1:]) - set(args)
if unknown := flags - {'--crop', '--dither', '--no-dither'}: sys.exit(f'unknown option {unknown.pop()}')
dither = True if '--dither' in flags else False if '--no-dither' in flags else None
if args: add(args, '--crop' in flags, dither)
else: rebuild(dither)
