# Runs a walkthrough like `devshot.py run`, but safe for Notes on a board with real keys:
# - X FAKE 1 is set first and never switched off (devshot's finish() turns it off, which would let the background
#   sync really send a waiting test note);
# - every note made during the run (ids not in N LIST before it) is deleted with N DEL at the end;
# - the one-off switches (EMPTY, FAILNOTE, NOSAVE, NETFAIL) are put back to 0; DEMO/DRY are left as they were.
#   python3 tools/safeshot.py <walkthrough> <out dir>      (test build on the board, over USB; nothing else on the port)
import glob, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import devshot


def note_ids(dev):
    dev.s.write(b'N LIST\n')
    ids, end = [], time.time() + 10
    while time.time() < end:
        l = dev.s.readline().decode(errors='replace').strip()
        if l.startswith('NF '):
            ids.append(l.split()[1])
        if l.startswith(('OK N LIST', 'ERR')):
            break
    return ids


def waiting(dev):
    dev.s.write(b'N ?\n')
    end = time.time() + 6
    while time.time() < end:
        l = dev.s.readline().decode(errors='replace').strip()
        if l.startswith('NC '):
            return l
    return '?'


walk, out = sys.argv[1], sys.argv[2]
dev = devshot.Device(sorted(glob.glob('/dev/cu.usbmodem*'))[0])
dev.cmd('X FAKE 1')
before = set(note_ids(dev))
run = devshot.Run(dev, out, None)
try:
    for line in open(walk):
        run.step(line.strip())
finally:
    dev.cmd('X FAKE 1')  # (a step may have set it to 0)
    for sw in ('EMPTY', 'FAILNOTE', 'NOSAVE', 'NETFAIL'):
        dev.cmd(f'X {sw} 0')
    new = [i for i in note_ids(dev) if i not in before]
    for i in new:
        dev.s.write(f'N DEL {i}\n'.encode())
        reply, end = '', time.time() + 5
        while time.time() < end and not reply.startswith(('OK N DEL', 'ERR')):
            reply = dev.s.readline().decode(errors='replace').strip()
        print('deleted test note', i, '->', reply)
    left = [i for i in note_ids(dev) if i not in before]
    print('test notes left:', left or 'none', '| card/notes/waiting:', waiting(dev))
    odd = [l for l in dev.log if not l.startswith(('NF ', 'NS ', 'NC ', 'OK N', 'ND '))]  # (my N reads aren't odd)
    if odd:
        print(f'{len(odd)} unexpected lines from the device:', *odd[-10:], sep='\n  ')
    for p in run.problems:
        print('PROBLEM:', p)
    print(f'{len(run.tiles)} shots in {out}')
