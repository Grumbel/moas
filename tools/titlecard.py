#!/usr/bin/env python3
"""Set the red "Stunden später..." / "Inzwischen..." title cards in other
languages.

The letters are hand drawn, so there is no font: each glyph is cut out of
the German or English card (red pixels, indices 19 and 20) and placed
again. The dark red shadow (17) is an extrusion of the red pixels to the
lower left (6 left, 2 down on bild 59; 4 left, 3 down on bild 11) and is
drawn anew. The background (blue gradient and stars) comes from the German
and English cards wherever either shows no text.

usage: titlecard.py SRCDIR ENDIR LANG OUTDIR
  SRCDIR  manick-src (bild/, phase/)
  ENDIR   lang/en (the English cards, used for letters and background)
  OUTDIR  lang/<code>; writes bild/bild_059.png, bild/bild_011.png and
          phase/phase_023.png (bild 59's text on index 0)
"""
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

RED = (19, 20)
SHADOW = 17
# the English cards use duplicate palette entries for some blues
SAME = {40: 112, 41: 117, 43: 127}
# phase 23 is bild 59's text cut out at this place
PHASE23 = (34, 50, 250, 93)

# glyph: (picture, box) -- the red components whose centre lies in the box
# (x0, x1, y0, y1); 'line' names the line the glyph sits on in its source
GLYPHS = {
    'S': ('de59', (40, 70, 50, 92), 'de59a'),
    'T': ('de59', (145, 185, 105, 139), 'de59b'),
    'U': ('de59', (115, 151, 53, 92), 'de59a'),
    'N': ('de59', (154, 182, 54, 93), 'de59a'),
    'D': ('de59', (185, 214, 53, 93), 'de59a'),
    'E': ('de59', (219, 244, 62, 94), 'de59a'),
    'P': ('de59', (78, 106, 97, 137), 'de59b'),
    'A': ('de59', (112, 145, 116, 141), 'de59b'),
    'R': ('de59', (214, 244, 106, 138), 'de59b'),
    'H': ('en59', (45, 85, 46, 94), 'en59a'),
    'O': ('en59', (90, 128, 48, 93), 'en59a'),
    'L': ('en59', (44, 78, 101, 145), 'en59b'),
    'M': ('en11', (13, 51, 78, 115), 'en11'),
    'C': ('de11', (165, 180, 90, 123), 'de11'),
    'I': ('de11', (126, 142, 70, 117), 'de11'),
    '.': ('de59', (243, 256, 126, 137), 'de59b'),
    'acute': ('de59', (107, 117, 103, 115), None),
}
# letters of a line, to find its baseline
LINES = {
    'de59a': ('de59', 'SUNDE'), 'de59b': ('de59', 'TPAR'),
    'en59a': ('en59', 'HO'), 'en59b': ('en59', 'L'),
    'en11': ('en11', 'M'), 'de11': ('de11', 'CI'),
}
ACCENTS = {'É': ('E', 'acute')}

# text per language: card -> lines of (text, x, baseline y at x, rise per
# pixel, gap between letters). The block is then centred: on bild 59 in
# the phase 23 window, on bild 11 horizontally on the screen.
TEXT = {
    'es': {59: [("HORAS", 48, 0, 0, 2), ("DESPUÉS...", 0, 43, 0, -2)],
           11: [("MIENTRAS", 0, 0, .1, 2), ("TANTO...", 40, 44, .1, 2)]},
    'fr': {59: [("PLUS", 0, 0, 0, 2), ("TARD...", 10, 43, 0, 2)],
           11: [("PENDANT", 0, 0, .1, 2), ("CE TEMPS...", 30, 44, .1, 2)]},
    'it': {59: [("ORE", 0, 0, 0, 2), ("DOPO...", 10, 43, 0, 2)],
           11: [("INTANTO...", 0, 0, .1, 2)]},
    'nl': {59: [("UREN", 0, 0, 0, 2), ("LATER...", 6, 43, 0, 2)],
           11: [("INTUSSEN...", 0, 0, .1, 2)]},
}
# vertical centre of the text on bild 11 (the German one sits at ~100)
CENTRE11 = 100
EXTRUDE = {59: 6, 11: 4}
DROP = {59: 2, 11: 3}
SPACE = 14


def load(path):
    im = Image.open(path)
    a = np.array(im)
    for k, v in SAME.items():
        a[a == k] = v
    return im, a


def cut(a, box):
    lab, _ = ndimage.label(np.isin(a, RED), structure=np.ones((3, 3)))
    x0, x1, y0, y1 = box
    keep = np.zeros(a.shape, bool)
    for i, sl in enumerate(ndimage.find_objects(lab), 1):
        cy = (sl[0].start + sl[0].stop - 1) / 2
        cx = (sl[1].start + sl[1].stop - 1) / 2
        if x0 <= cx < x1 and y0 <= cy < y1:
            keep |= lab == i
    ys, xs = np.nonzero(keep)
    return {(x - xs.min(), y - ys.max()): a[y, x] for x, y in zip(xs, ys)}, \
        (xs.min(), xs.max(), ys.max())


def shadow_offsets(card):
    n, d = EXTRUDE[card], DROP[card]
    return [(-k, int(k * d / n + 0.5)) for k in range(1, n + 1)]


def main(srcdir, endir, lang, outdir):
    pics = {'de59': load(f"{srcdir}/bild/bild_059.png"),
            'de11': load(f"{srcdir}/bild/bild_011.png"),
            'en59': load(f"{endir}/bild/bild_059.png"),
            'en11': load(f"{endir}/bild/bild_011.png")}
    raw = {c: (pics[src][1], box, line) for c, (src, box, line) in GLYPHS.items()}
    cuts = {c: cut(a, box) for c, (a, box, _) in raw.items()}
    # baseline of each source line (straight line through the letter feet)
    base = {}
    for name, (src, letters) in LINES.items():
        pts = [((cuts[c][1][0] + cuts[c][1][1]) / 2, cuts[c][1][2]) for c in letters]
        xs, ys = zip(*pts)
        k = np.polyfit(xs, ys, 1) if len(pts) > 1 else (0, ys[0])
        base[name] = lambda x, k=k: k[0] * x + k[1]
    glyphs = {}
    for c, (pix, (x0, x1, bottom)) in cuts.items():
        line = GLYPHS[c][2]
        dy = bottom - base[line]((x0 + x1) / 2) if line else 0
        glyphs[c] = (pix, x1 - x0 + 1, round(dy))

    for card in (59, 11):
        name = f"bild_{card:03d}.png"
        im, de = pics[f'de{card}']
        en = pics[f'en{card}'][1]
        bg = np.where(np.isin(de, RED + (SHADOW,)), en, de)
        hole = np.isin(bg, RED + (SHADOW,))
        for y, x in zip(*np.nonzero(hole)):
            for d in range(2, 320, 2):
                for xx in (x - d, x + d):
                    if 0 <= xx < 320 and not hole[y, xx]:
                        bg[y, x] = bg[y, xx]
                        break
                else:
                    continue
                break
        red = {}
        for text, x, y, rise, gap in TEXT[lang][card]:
            x0 = x
            for ch in text:
                if ch == ' ':
                    x += SPACE
                    continue
                c, mark = ACCENTS.get(ch, (ch, None))
                pix, w, dy = glyphs[c]
                by = round(y + rise * (x - x0)) + dy
                for (gx, gy), v in pix.items():
                    red[(x + gx, by + gy)] = v
                if mark:
                    top = min(gy for _, gy in pix)
                    mpix, mw, _ = glyphs[mark]
                    mx = x + (w - mw) // 2 + 3
                    for (gx, gy), v in mpix.items():
                        red[(mx + gx, by + top - 3 + gy)] = v
                x += w + gap
        xs = [x for x, _ in red]
        ys = [y for _, y in red]
        n = EXTRUDE[card]
        if card == 59:
            px, py, pw, ph = PHASE23
            cx, cy = px + pw / 2, py + ph / 2
        else:
            cx, cy = 160, CENTRE11
        # centre including the shadow (n left, DROP down)
        sx = round(cx - (min(xs) - n + max(xs) + 1) / 2)
        sy = round(cy - (min(ys) + max(ys) + DROP[card] + 1) / 2)
        red = {(x + sx, y + sy): v for (x, y), v in red.items()}
        out = bg.copy()
        for (x, y) in red:
            for dx, dy in shadow_offsets(card):
                if 0 <= x + dx < 320 and 0 <= y + dy < 200:
                    out[y + dy, x + dx] = SHADOW
        for (x, y), v in red.items():
            if not (0 <= x < 320 and 0 <= y < 200):
                sys.exit(f"{lang} {name}: text outside the picture at {x},{y}")
            out[y, x] = v
        res = Image.fromarray(out, 'P')
        res.putpalette(im.getpalette())
        os.makedirs(f"{outdir}/bild", exist_ok=True)
        res.save(f"{outdir}/bild/{name}")
        if card == 59:
            px, py, pw, ph = PHASE23
            text = np.isin(out, RED + (SHADOW,))
            ys, xs = np.nonzero(text)
            if xs.min() < px or xs.max() >= px + pw or ys.min() < py or ys.max() >= py + ph:
                sys.exit(f"{lang} {name}: text leaves the phase 23 window "
                         f"({xs.min()}-{xs.max()}, {ys.min()}-{ys.max()})")
            ph23 = np.where(text, out, 0)[py:py + ph, px:px + pw]
            p = Image.fromarray(np.ascontiguousarray(ph23), 'P')
            p.putpalette(Image.open(f"{srcdir}/phase/phase_023.png").getpalette())
            os.makedirs(f"{outdir}/phase", exist_ok=True)
            p.save(f"{outdir}/phase/phase_023.png", transparency=0)


if __name__ == '__main__':
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    main(*sys.argv[1:])
