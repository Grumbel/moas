#!/usr/bin/env python3
"""Translate the text baked into MOAS phase pictures (help pages, credits).

The text in these pictures is one palette colour (yellow 51, red 19) plus a
shadow in index 1. The shadow is always a fixed dilation of the text pixels
(see KERNELS), so a font only needs the text pixels of each glyph; the
shadow is added when rendering.

  phasefont.py lines PNG...
      Show the text lines found in each picture (index, position, shadow
      style, glyph runs per word). Translations refer to these indices.
  phasefont.py extract TRANSCRIPT SRCDIR FONT
      Cut glyphs out of the pictures listed in TRANSCRIPT and write them as
      editable ASCII art to FONT. This made tools/phasetext/help.font,
      which was then fixed and extended by hand.
  phasefont.py accents FONT
      Add the accented letters FONT lacks (áàâä..., ñ, ç, ¡, ¿), built from
      the base letter and an accent mark (MARKS). Edit them by hand after.
  phasefont.py render FONT SPEC SRCDIR OUTDIR
      Erase the text of each picture named in SPEC and draw the translated
      lines instead. Graphics stay where they are. Warns (and exits 1) when
      a line leaves the picture, hits graphics or needs a missing glyph.

A new language needs only a SPEC file, e.g. for English:
  tools/phasefont.py render tools/phasetext/help.font \\
      tools/phasetext/en.txt manick-src/phase lang/en/phase
where manick-src/phase comes from `moas-util unpack`. Rendering
tools/phasetext/de.txt gives back the originals (close to, not exactly:
the original has some hand-tuned spacing).

File formats are described above the functions that read them.
Needs Pillow.
"""
import os
import re
import sys
from collections import Counter
from PIL import Image

SHADOW = 1
TEXT_COLOURS = (51, 19)

# offsets from a text pixel that are shadow
KERNELS = {
    # help pages, most text
    'thick': [(0, -1), (1, -1), (-1, 0), (1, 0), (2, 0), (-1, 1), (0, 1),
              (1, 1), (2, 1), (0, 2), (1, 2)],
    # outline with one pixel drop (labels on phase 53, body text on 59)
    'thin': [(0, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1), (0, 2)],
    # credits
    'heavy': [(dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1, 2)
              if (dx, dy) != (0, 0)],
    'heavier': [(dx, dy) for dx in (-1, 0, 1, 2) for dy in (-1, 0, 1, 2)
                if (dx, dy) != (0, 0)],
    # red title words (phases 34-36)
    'red': [(0, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1), (2, 1),
            (0, 2), (1, 2)],
}

N8 = [(dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy]
N4 = [(1, 0), (-1, 0), (0, 1), (0, -1)]


def load(path):
    im = Image.open(path)
    if im.mode != 'P':
        sys.exit(f"{path}: not an indexed PNG")
    return im


def components_of(pts, neighbours=N8):
    """Connected components of a set of points."""
    left = set(pts)
    out = []
    while left:
        stack = [left.pop()]
        comp = list(stack)
        while stack:
            x, y = stack.pop()
            for dx, dy in neighbours:
                q = (x + dx, y + dy)
                if q in left:
                    left.remove(q)
                    comp.append(q)
                    stack.append(q)
        out.append(comp)
    return out


def bbox(pts):
    xs = [x for x, _ in pts]
    ys = [y for _, y in pts]
    return min(xs), min(ys), max(xs), max(ys)


class Line:
    def __init__(self):
        self.comps = []
        self.underline = None   # list of pixels
        self.style = None

    @property
    def pixels(self):
        return [q for c in self.comps for q in c]

    def box(self):
        return bbox(self.pixels)

    def runs(self):
        """Column runs of text pixels: [(x0, x1)], separated by empty columns."""
        cols = sorted({x for x, _ in self.pixels})
        runs = []
        for x in cols:
            if runs and x == runs[-1][1] + 1:
                runs[-1][1] = x
            else:
                runs.append([x, x])
        return [tuple(r) for r in runs]

    def baseline(self):
        """Most common bottom row of the glyph runs."""
        pix = self.pixels
        bottoms = Counter()
        for x0, x1 in self.runs():
            bottoms[max(y for x, y in pix if x0 <= x <= x1)] += 1
        return bottoms.most_common(1)[0][0]


def is_text_comp(pix, w, h, comp, colour):
    """Text glyphs touch only transparent, shadow and their own colour."""
    for x, y in comp:
        for dx, dy in N8:
            q = (x + dx, y + dy)
            if 0 <= q[0] < w and 0 <= q[1] < h and pix[q] not in (0, SHADOW, colour):
                return False
    return True


def find_lines(im):
    """Group the text components of a picture into lines, top to bottom."""
    w, h = im.size
    pix = im.load()
    colour = max(TEXT_COLOURS, key=lambda c: sum(1 for v in im.getdata() if v == c))
    comps = [c for c in components_of([(x, y) for y in range(h) for x in range(w)
                                       if pix[x, y] == colour])
             if is_text_comp(pix, w, h, c, colour)]
    unders = []
    glyphs = []
    tall = []       # touching glyphs of two lines
    for c in comps:
        x0, y0, x1, y1 = bbox(c)
        rows = Counter(y for _, y in c)
        if x1 - x0 >= 20 and rows.most_common(1)[0][1] >= 20:
            # heading underline (two rows), maybe with descenders touching it
            uy = max(rows, key=lambda y: (rows[y], y))
            urows = {y for y in (uy - 1, uy, uy + 1) if rows.get(y, 0) >= 20}
            unders.append([q for q in c if q[1] in urows])
            c = [q for q in c if q[1] not in urows]
        # split by 4-connectivity: a descender may touch the line below
        # diagonally
        for g in components_of(c, N4):
            x0, y0, x1, y1 = bbox(g)
            if x1 - x0 <= 24 and y1 - y0 <= 13:
                glyphs.append(g)
            elif x1 - x0 <= 24 and y1 - y0 <= 30:
                tall.append(g)
    # letters (at least 5 rows) sharing 3 rows and close horizontally are on
    # one line; dots, commas and accents join the line they sit on
    big = [c for c in glyphs if bbox(c)[3] - bbox(c)[1] >= 4]
    small = [c for c in glyphs if bbox(c)[3] - bbox(c)[1] < 4]
    boxes = [bbox(c) for c in big]
    parent = list(range(len(big)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    for i, (ax0, ay0, ax1, ay1) in enumerate(boxes):
        for j in range(i):
            bx0, by0, bx1, by1 = boxes[j]
            if (min(ay1, by1) - max(ay0, by0) >= 2
                    and max(ax0, bx0) - min(ax1, bx1) <= 30):
                parent[find(i)] = find(j)
    groups = {}
    for i, c in enumerate(big):
        groups.setdefault(find(i), []).append(c)
    lines = []
    for comps in groups.values():
        ln = Line()
        ln.comps = comps
        ln.colour = colour
        lines.append(ln)
    # small parts go to the line whose letter body (baseline-6..baseline)
    # is closest; dots sit above it, commas and descender bits below
    # split glyphs of two lines that touch: rows down to 3 below the upper
    # line's baseline belong to it
    for c in tall:
        x0, y0, x1, y1 = bbox(c)
        hit = sorted((ln.baseline(), ln) for ln in lines
                     if y0 <= ln.baseline() <= y1
                     and ln.box()[0] - 4 <= x1 and x0 <= ln.box()[2] + 4)
        if not hit:
            continue
        for k, (base, ln) in enumerate(hit):
            top = hit[k - 1][0] + 3 if k else -1
            bottom = base + 3 if k + 1 < len(hit) else y1
            part = [q for q in c if top < q[1] <= bottom]
            if part:
                ln.comps.append(part)
    # leader lines and other thin strokes are not text
    lines = [ln for ln in lines if ln.box()[2] - ln.box()[0] >= 3]
    bases = [(ln, ln.baseline()) for ln in lines]
    while small:
        left = []
        for c in small:
            x0, y0, x1, y1 = bbox(c)
            cy = (y0 + y1) / 2
            best = None
            for ln, base in bases:
                lx0, _, lx1, _ = ln.box()
                if x1 < lx0 - 4 or x0 > lx1 + 4:
                    continue
                d = max(base - 6 - cy, cy - base, 0)
                if d <= 6 and (best is None or d < best[0]):
                    best = (d, ln)
            if best:
                best[1].comps.append(c)
            else:
                left.append(c)
        if len(left) == len(small):
            break
        small = left
    lines = [ln for ln in lines if ln.box()[3] - ln.box()[1] >= 6]
    lines.sort(key=lambda ln: (ln.box()[1], ln.box()[0]))
    for u in unders:
        ux0, uy0, ux1, _ = bbox(u)
        for ln in lines:
            x0, y0, x1, y1 = ln.box()
            if 0 < uy0 - ln.baseline() <= 5 and ux0 < x1 and ux1 > x0:
                ln.underline = u
    for ln in lines:
        ln.style = guess_style(im, ln)
    return lines


def guess_style(im, ln):
    """The kernel whose shadow fits the line's pixels best."""
    w, h = im.size
    pix = im.load()
    text = set(ln.pixels)
    best = None
    for name, k in KERNELS.items():
        if name == 'red' and ln.colour != 19:
            continue
        cov = {(x + dx, y + dy) for x, y in text for dx, dy in k} - text
        cov = {q for q in cov if 0 <= q[0] < w and 0 <= q[1] < h}
        miss = sum(1 for q in cov if pix[q] not in (SHADOW, ln.colour))
        score = (miss, -len(cov))
        if best is None or score < best[0]:
            best = (score, name)
    return best[1]


def words_of(runs, gap=4):
    """Split column runs into words at gaps of at least `gap` columns."""
    words = []
    for r in runs:
        if words and r[0] - words[-1][-1][1] - 1 < gap:
            words[-1].append(r)
        else:
            words.append([r])
    return words


def cmd_lines(paths):
    for path in paths:
        im = load(path)
        print(f"== {os.path.basename(path)}")
        for i, ln in enumerate(find_lines(im)):
            x0, y0, x1, y1 = ln.box()
            ws = words_of(ln.runs())
            u = ' underline' if ln.underline else ''
            print(f"{i:2d}: x={x0}-{x1} y={y0}-{y1} base={ln.baseline()} "
                  f"{ln.style}{u}  " + ' '.join(str(len(wd)) for wd in ws))


# ---------------------------------------------------------------- spec files
#
# Transcripts and translations share one format:
#
#   == phase_054.png
#   0: Der Mauscursor
#   1: Mit der Maus neben ihrem Computer
#
# The number is the line index from `phasefont.py lines`. Lines not listed
# are left alone. Translations may add options before the text:
#   3 x=120 y=+2: text     move the line (x absolute, y relative to its
#                          original baseline, both optional)
#   3+: text               an extra line below line 3, one line pitch further
#   3: -                   erase line 3 and draw nothing
# '#' starts a comment line.

LINE_RE = re.compile(r'^(\d+)(\+*)((?:\s+[xy]=[+-]?\d+)*):\s?(.*)$')


def read_spec(path):
    spec = {}
    cur = None
    with open(path, encoding='utf-8') as f:
        for n, raw in enumerate(f, 1):
            line = raw.rstrip('\n')
            if not line.strip() or line.startswith('#'):
                continue
            if line.startswith('== '):
                cur = line[3:].strip()
                spec[cur] = []
                continue
            m = LINE_RE.match(line)
            if not m or cur is None:
                sys.exit(f"{path}:{n}: cannot parse: {line}")
            opts = dict(o.split('=') for o in m.group(3).split())
            spec[cur].append((int(m.group(1)), len(m.group(2)), opts, m.group(4)))
    return spec


# ---------------------------------------------------------------- fonts
#
# A font file holds one block per glyph, separated by blank lines:
#
#   glyph j advance 3 offset -2
#    -9 ...Y
#    -8 ..YY
#   ...
#     0 ..YY
#     3 YY
#
# Each row starts with its distance from the baseline (0 = the row the
# letters stand on, negative = up); rows without ink can be left out. 'Y' is
# ink, anything else is not. The glyph is drawn `offset` columns right of
# the pen (default 0), and the pen then moves `advance` columns. 'space'
# names the space character. Lines starting with '#' are comments.

class Glyph:
    def __init__(self, ink, advance=None, offset=0):
        self.ink = frozenset(ink)       # (dx, dy), dy relative to baseline
        self.width = max(x for x, _ in ink) + 1 if ink else 0
        self.advance = advance if advance is not None else self.width + 1
        self.offset = offset


def normalise(ink):
    x0 = min(x for x, _ in ink)
    return frozenset((x - x0, y) for x, y in ink)


def write_font(path, font, header=''):
    with open(path, 'w', encoding='utf-8') as f:
        f.write(header)
        for ch in sorted(font, key=ord):
            g = font[ch]
            name = 'space' if ch == ' ' else ch
            off = f" offset {g.offset}" if g.offset else ''
            f.write(f"glyph {name} advance {g.advance}{off}\n")
            ys = sorted({y for _, y in g.ink})
            for y in range(ys[0], ys[-1] + 1) if ys else ():
                row = ''.join('Y' if (x, y) in g.ink else '.'
                              for x in range(g.width)).rstrip('.')
                f.write(f"{y:3d} {row}\n")
            f.write('\n')


def read_font(path):
    font = {}
    cur = None
    with open(path, encoding='utf-8') as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip('\n')
            if not line.strip() or line.startswith('#'):
                continue
            if line.startswith('glyph '):
                w = line.split()
                if len(w) < 4 or w[2] != 'advance':
                    sys.exit(f"{path}:{n}: bad glyph header")
                opts = dict(zip(w[2::2], map(int, w[3::2])))
                cur = ([], opts)
                font[' ' if w[1] == 'space' else w[1]] = cur
                continue
            m = re.match(r'^\s*(-?\d+) ?(.*)$', line)
            if not m or cur is None:
                sys.exit(f"{path}:{n}: cannot parse: {line}")
            y = int(m.group(1))
            cur[0].extend((x, y) for x, c in enumerate(m.group(2)) if c == 'Y')
    return {c: Glyph(ink, o['advance'], o.get('offset', 0))
            for c, (ink, o) in font.items()}


# ---------------------------------------------------------------- extract

def line_ink(ln):
    """Ink of a line relative to (first column, baseline)."""
    base = ln.baseline()
    return {(x, y - base) for x, y in ln.pixels}


def fit_line(ink, text, templates, max_unknown):
    """Place the glyphs of `text` on `ink` (set of (x, y)).

    Known glyphs are matched against their template, unknown ones take
    whatever ink lies in their columns. Returns [(char, x, width)] or None.
    """
    cols = {}
    for x, y in ink:
        cols.setdefault(x, set()).add(y)
    x_end = max(cols) + 1
    chars = [c for c in text]
    unknown = sum(1 for c in chars if c != ' ' and c not in templates)
    if unknown > max_unknown:
        return None
    INF = float('inf')

    def place_cost(c, x, w):
        if c in templates:
            t = templates[c].ink
            cost = 0
            for dx in range(w):
                have = cols.get(x + dx, set())
                want = {y for tx, y in t if tx == dx}
                cost += len(have ^ want)
            return cost
        return 0.5 * w

    def gap_cost(x0, x1):
        return sum(len(cols.get(x, ())) for x in range(x0, x1))

    # best[(i, x)] = (cost, back) where glyph i (index into non-space chars)
    # starts at column x
    glyphs = [(k, c) for k, c in enumerate(chars) if c != ' ']
    best = {(0, 0): (0, None)}
    for gi, (k, c) in enumerate(glyphs):
        widths = [templates[c].width] if c in templates else range(1, 15)
        nxt_space = gi + 1 < len(glyphs) and ' ' in chars[k + 1:glyphs[gi + 1][0]]
        gaps = range(2, 16) if nxt_space else range(-1, 4)
        for (i, x), (cost, _) in list(best.items()):
            if i != gi:
                continue
            for w in widths:
                pc = place_cost(c, x, w)
                if gi + 1 == len(glyphs):
                    if x + w == x_end:
                        key = (gi + 1, x_end)
                        tot = cost + pc
                        if tot < best.get(key, (INF,))[0]:
                            best[key] = (tot, (gi, x, w))
                    continue
                for g in gaps:
                    nx = x + w + g
                    if nx >= x_end:
                        break
                    tot = cost + pc + gap_cost(x + w, nx)
                    # the next glyph must start on ink
                    if not cols.get(nx):
                        continue
                    key = (gi + 1, nx)
                    if tot < best.get(key, (INF,))[0]:
                        best[key] = (tot, (gi, x, w))
    end = best.get((len(glyphs), x_end))
    if end is None:
        return None
    out = []
    key = (len(glyphs), x_end)
    while best[key][1] is not None:
        gi, x, w = best[key][1]
        out.append((glyphs[gi][1], x, w))
        key = (gi, x)
    out.reverse()
    return end[0], out


def cmd_extract(transcript, srcdir, fontpath):
    spec = read_spec(transcript)
    data = []   # (ink relative to line start, text)
    for name, entries in spec.items():
        lines = find_lines(load(os.path.join(srcdir, name)))
        for idx, _, _, text in entries:
            ink = line_ink(lines[idx])
            x0 = min(x for x, _ in ink)
            data.append(({(x - x0, y) for x, y in ink}, text, f"{name}:{idx}"))

    # bootstrap from words whose column runs match their letters
    votes = {}
    for ink, text, _ in data:
        xs = sorted({x for x, _ in ink})
        runs = []
        for x in xs:
            if runs and x == runs[-1][1] + 1:
                runs[-1][1] = x
            else:
                runs.append([x, x])
        words = words_of([tuple(r) for r in runs])
        tw = text.split()
        if len(words) != len(tw):
            continue
        for wd, t in zip(words, tw):
            if len(wd) != len(t):
                continue
            for (a, b), c in zip(wd, t):
                g = normalise({(x, y) for x, y in ink if a <= x <= b})
                votes.setdefault(c, Counter())[g] += 1

    def templates_from(votes):
        return {c: Glyph(v.most_common(1)[0][0]) for c, v in votes.items()}

    templates = templates_from(votes)
    fitted = {}
    for max_unknown in (1, 1, 2, 2, 3, 3, 4, 4, 4):
        votes = {}
        gaps = {}       # gap after a glyph inside a word
        spaces = Counter()
        for ink, text, where in data:
            r = fit_line(ink, text, templates, max_unknown)
            if r is None:
                continue
            cost, placed = r
            fitted[where] = cost
            words = text.split()
            k = 0
            for word in words:
                for j in range(len(word)):
                    c, x, w = placed[k]
                    g = normalise({(px, py) for px, py in ink if x <= px < x + w})
                    votes.setdefault(c, Counter())[g] += 1
                    if k + 1 < len(placed):
                        gap = placed[k + 1][1] - x - w
                        if j + 1 < len(word):
                            gaps.setdefault(c, Counter())[gap] += 1
                        else:
                            spaces[(c, gap)] += 1
                    k += 1
        templates.update(templates_from(votes))
    missing = [where for _, _, where in data if where not in fitted]
    print(f"fitted {len(fitted)} of {len(data)} lines; not fitted: {missing}")
    bad = sorted(((c, w) for w, c in fitted.items() if c > 0), reverse=True)
    print("lines with mismatching pixels:", bad[:20])
    font = {}
    for c, t in templates.items():
        gap = gaps[c].most_common(1)[0][0] if c in gaps else 1
        font[c] = Glyph(t.ink, t.width + gap)
        v = votes.get(c, Counter())
        print(f"  {c!r}: {sum(v.values())} samples, {len(v)} variants, "
              f"top {v.most_common(1)[0][1] if v else 0}, gaps {dict(gaps.get(c, {}))}")
    # a space adds to the advance of the glyph before it
    extra = Counter()
    for (c, gap), n in spaces.items():
        extra[gap - (font[c].advance - font[c].width)] += n
    font[' '] = Glyph((), extra.most_common(1)[0][0])
    print("space widths", dict(extra))
    write_font(fontpath, font)


# ---------------------------------------------------------------- accents

# accent marks, drawn above the letter body (or below for the cedilla)
MARKS = {
    'acute': ["..YY", ".YY"],
    'grave': ["YY", ".YY"],
    'circ': [".YYY", "YY.YY"],
    'diaer': ["YY..YY", "YY..YY"],
    'diaer_i': ["Y..Y", "Y..Y"],
    'tilde': [".YY..Y", "Y..YY"],
    'cedilla': ["..Y", ".YY"],
}

ACCENTED = {
    'á': ('a', 'acute'), 'à': ('a', 'grave'), 'â': ('a', 'circ'), 'ä': ('a', 'diaer'),
    'é': ('e', 'acute'), 'è': ('e', 'grave'), 'ê': ('e', 'circ'), 'ë': ('e', 'diaer'),
    'í': ('i', 'acute'), 'ì': ('i', 'grave'), 'î': ('i', 'circ'), 'ï': ('i', 'diaer_i'),
    'ó': ('o', 'acute'), 'ò': ('o', 'grave'), 'ô': ('o', 'circ'), 'ö': ('o', 'diaer'),
    'ú': ('u', 'acute'), 'ù': ('u', 'grave'), 'û': ('u', 'circ'), 'ü': ('u', 'diaer'),
    'ñ': ('n', 'tilde'), 'ç': ('c', 'cedilla'), 'ÿ': ('y', 'diaer'),
    'Á': ('A', 'acute'), 'À': ('A', 'grave'), 'Â': ('A', 'circ'), 'Ä': ('A', 'diaer'),
    'É': ('E', 'acute'), 'È': ('E', 'grave'), 'Ê': ('E', 'circ'), 'Ë': ('E', 'diaer'),
    'Í': ('I', 'acute'), 'Ì': ('I', 'grave'), 'Î': ('I', 'circ'), 'Ï': ('I', 'diaer_i'),
    'Ó': ('O', 'acute'), 'Ò': ('O', 'grave'), 'Ô': ('O', 'circ'), 'Ö': ('O', 'diaer'),
    'Ú': ('U', 'acute'), 'Ù': ('U', 'grave'), 'Û': ('U', 'circ'), 'Ü': ('U', 'diaer'),
    'Ñ': ('N', 'tilde'), 'Ç': ('C', 'cedilla'),
}


def compose(base, mark, base_is_lower):
    """Put an accent mark on a glyph, centred on the letter body."""
    upper = min(y for _, y in base.ink) <= -8 and not base_is_lower
    # the i's own dot goes; lowercase bodies start at row -6
    body = {(x, y) for x, y in base.ink if y >= -6} if not upper else set(base.ink)
    rows = MARKS[mark]
    mw = max(len(r) for r in rows)
    bx = [x for x, _ in body]
    left = round((min(bx) + max(bx) + 1 - mw) / 2)
    if mark == 'cedilla':
        top = 1
    else:
        top = (-12 if upper else -9)
    ink = set(body)
    for dy, r in enumerate(rows):
        for dx, c in enumerate(r):
            if c == 'Y':
                ink.add((left + dx, top + dy))
    shift = min(x for x, _ in ink)
    ink = {(x - shift, y) for x, y in ink}
    return Glyph(ink, base.advance, base.offset + shift)


def flipped(g, dy):
    """Upside-down glyph moved down by dy (for ¡ and ¿)."""
    w = g.width
    ink = {(w - 1 - x, -9 - y + dy) for x, y in g.ink}
    return Glyph(ink, g.advance, g.offset)


def cmd_accents(fontpath):
    """Add accented letters that the font lacks, built from base glyphs."""
    font = read_font(fontpath)
    added = {}
    for ch, (base, mark) in ACCENTED.items():
        if ch not in font and base in font:
            added[ch] = compose(font[base], mark, base.islower())
    for ch, src in (('¡', '!'), ('¿', '?')):
        if ch not in font and src in font:
            added[ch] = flipped(font[src], 3)
    with open(fontpath, encoding='utf-8') as f:
        text = f.read()
    tmp = fontpath + '.tmp'
    write_font(tmp, added)
    with open(tmp, encoding='utf-8') as f:
        extra = f.read()
    os.remove(tmp)
    with open(fontpath, 'w', encoding='utf-8') as f:
        f.write(text.rstrip('\n') + '\n\n' + extra)
    print("added", ''.join(sorted(added)))


# ---------------------------------------------------------------- render

def text_ink(font, text):
    """Ink of a text line, pen starting at 0, baseline at 0."""
    ink = set()
    pen = 0
    for c in text:
        g = font.get(c)
        if g is None:
            raise KeyError(c)
        ink |= {(pen + g.offset + x, y) for x, y in g.ink}
        pen += g.advance
    return ink


def underline_pattern(ln):
    """(ink relative to underline start and baseline, period, left, right)."""
    base = ln.baseline()
    u = ln.underline
    ux0 = min(x for x, _ in u)
    ux1 = max(x for x, _ in u)
    ink = {(x - ux0, y - base) for x, y in u}
    width = ux1 - ux0 + 1
    period = next((p for p in range(2, 16)
                   if all(((x + p, y) in ink) == ((x, y) in ink)
                          for x in range(width - p) for y in (-1, 0, 1, 2, 3, 4, 5))),
                  width)
    tile = {(x, y) for x, y in ink if x < period}
    x0, _, x1, _ = ln.box()
    return tile, period, ux0 - x0, ux1 - x1


def erase_lines(im, lines):
    w, h = im.size
    pix = im.load()
    for ln in lines:
        k = KERNELS[ln.style]
        for x, y in set(ln.pixels) | set(ln.underline or ()):
            pix[x, y] = 0
            for dx, dy in k:
                q = (x + dx, y + dy)
                if 0 <= q[0] < w and 0 <= q[1] < h and pix[q] == SHADOW:
                    pix[q] = 0
    # shadow specks left over (hand edits in the original) next to erased
    # text, not touching anything else
    pts = [(x, y) for ln in lines for x, y in
           [(x, y) for x in range(ln.box()[0] - 3, ln.box()[2] + 4)
            for y in range(ln.box()[1] - 3, ln.box()[3] + 6)]
           if 0 <= x < w and 0 <= y < h and pix[x, y] == SHADOW]
    for comp in components_of(pts):
        touches = False
        for x, y in comp:
            for dx, dy in N8:
                q = (x + dx, y + dy)
                if 0 <= q[0] < w and 0 <= q[1] < h and pix[q] not in (0, SHADOW):
                    touches = True
        if not touches:
            for q in comp:
                pix[q] = 0


def draw(im, ink, colour, style, where, warn):
    w, h = im.size
    pix = im.load()
    k = KERNELS[style]
    for x, y in sorted(ink):
        if not (0 <= x < w and 0 <= y < h):
            warn(f"{where}: text outside the picture at {x},{y}")
            return
        if pix[x, y] not in (0, SHADOW):
            warn(f"{where}: text over graphics at {x},{y}")
            return
    for x, y in ink:
        pix[x, y] = colour
    hits = 0
    for x, y in ink:
        for dx, dy in k:
            q = (x + dx, y + dy)
            if 0 <= q[0] < w and 0 <= q[1] < h:
                if pix[q] == 0:
                    pix[q] = SHADOW
                elif pix[q] not in (SHADOW, colour):
                    hits += 1
    if hits:
        warn(f"{where}: shadow touches graphics ({hits} pixels)")


def line_pitch(lines):
    diffs = Counter()
    bases = sorted(ln.baseline() for ln in lines)
    for a, b in zip(bases, bases[1:]):
        if 10 <= b - a <= 20:
            diffs[b - a] += 1
    return diffs.most_common(1)[0][0] if diffs else 13


def render_phase(im, font, entries, where, warn):
    lines = find_lines(im)
    erase_lines(im, [lines[i] for i in sorted({e[0] for e in entries})])
    pitch = line_pitch(lines)
    prev = {}
    for idx, extra, opts, text in entries:
        ln = lines[idx]
        x0, _, _, _ = ln.box()
        base = ln.baseline()
        if extra:
            base = prev[idx][1] + pitch
            x0 = prev[idx][0]
        x0 = int(opts.get('x', x0))
        base += int(opts.get('y', 0)) if not extra else 0
        prev[idx] = (x0, base)
        if text == '-':
            continue
        try:
            ink = text_ink(font, text)
        except KeyError as e:
            warn(f"{where}:{idx}: no glyph for {e.args[0]!r}")
            continue
        ink = {(x0 + x, base + y) for x, y in ink}
        if ln.underline and not extra:
            tile, period, left, right = underline_pattern(ln)
            tx0 = min(x for x, _ in ink)
            tx1 = max(x for x, _ in ink)
            for ux in range(tx0 + left, tx1 + right + 1):
                rel = ux - (tx0 + left)
                ink |= {(ux, base + y) for x, y in tile if x == rel % period}
        draw(im, ink, ln.colour, ln.style, f"{where}:{idx}", warn)


def cmd_render(fontpath, specpath, srcdir, outdir):
    font = read_font(fontpath)
    spec = read_spec(specpath)
    warnings = []
    for name, entries in spec.items():
        im = load(os.path.join(srcdir, name))
        render_phase(im, font, entries, name, warnings.append)
        im.save(os.path.join(outdir, name))
    for wmsg in warnings:
        print(wmsg)
    return 1 if warnings else 0


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd == 'lines':
        cmd_lines(args)
    elif cmd == 'extract' and len(args) == 3:
        cmd_extract(*args)
    elif cmd == 'accents' and len(args) == 1:
        cmd_accents(*args)
    elif cmd == 'render' and len(args) == 4:
        sys.exit(cmd_render(*args))
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
