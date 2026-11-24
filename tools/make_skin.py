#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 bokontep
r"""Generate the Polivoks 1982 MPC touch skin and a local preview.

This is a development tool, not part of the build.  It uses Akai's
tools/gen_ui.py to produce a valid baseline TUI/Q-Links pair, then replaces
the kit artwork and adds a full-panel background to every page.

Usage:
  python3 tools/make_skin.py --sdk .sdk --params .sdk/plugins/Polivoks\ 1982/build/params.json
"""

import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont, ImageFilter

try:
    import numpy as np
except ImportError:
    np = None

# ---------------------------------------------------------------------------
# paths / constants
# ---------------------------------------------------------------------------
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLUGIN = os.path.join(ROOT, "plugin", "Polivoks 1982")
SKIN = os.path.join(PLUGIN, "skins")
UI = os.path.join(PLUGIN, "ui.json")

SLOT_X = [23, 343, 663, 983]
SLOT_Y = [45, 196, 346, 497]
SLOT_W, SLOT_H = 278, 107
NEW_Y = [95, 225, 355, 485]
PLATE_Y = 44
PLATE_H = 580

S = 2  # supersampling for the panel artwork

FONT_DIR = "/usr/share/fonts/truetype/dejavu"
FONT_BOLD = os.path.join(FONT_DIR, "DejaVuSans-Bold.ttf")
FONT_REG = os.path.join(FONT_DIR, "DejaVuSans.ttf")
FONT_COND = os.path.join(FONT_DIR, "DejaVuSansCondensed-Bold.ttf")

PAGES = [
    {
        "name": "VCO",
        "ru": "ГЕНЕРАТОРЫ · МИКШЕР",
        "en": "GENERATORS · MIXER",
        "modules": [
            ("ГЕНЕРАТОР I", "VCO I", "vco"),
            ("ГЕНЕРАТОР II", "VCO II", "vco"),
            ("МИКШЕР", "MIXER", "mixer"),
            ("МАСТЕР", "MASTER", "master"),
        ],
    },
    {
        "name": "FILTER",
        "ru": "ФИЛЬТР · ОГИБАЮЩАЯ",
        "en": "FILTER · ENVELOPE",
        "modules": [
            ("ФИЛЬТР", "VCF", "filter"),
            ("ФИЛЬТР ЭГ", "FILTER ADSR", "adsr"),
            ("РЕЖИМ", "MODE", "mode"),
            ("ВЫХОД", "OUTPUT", "out"),
        ],
    },
    {
        "name": "AMP",
        "ru": "УСИЛИТЕЛЬ · ГОЛОС",
        "en": "AMPLIFIER · VOICE",
        "modules": [
            ("УСИЛ. ЭГ", "AMP ADSR", "adsr"),
            ("УСИЛИТЕЛЬ", "VCA", "vca"),
            ("ГОЛОС", "VOICE", "voice"),
            ("ДОПОЛНИТЕЛЬНО", "EXTRAS", "extras"),
        ],
    },
    {
        "name": "LFO",
        "ru": "МОДУЛЯТОР · НАСТРОЙКА",
        "en": "MODULATOR · TUNING",
        "modules": [
            ("МОДУЛЯТОР", "LFO", "lfo"),
            ("МОДУЛЯЦИЯ", "ROUTING", "routing"),
            ("НАСТРОЙКА", "TUNING", "tune"),
            ("РАСШИРЕНИЕ", "EXTRAS", "extras"),
        ],
    },
]

# ---------------------------------------------------------------------------
# tiny ordered-pairs JSON (TUI.json has duplicate "version" keys)
# ---------------------------------------------------------------------------
class Obj(list):
    pass


def obj(*pairs):
    return Obj(pairs)


def dump_json(value, indent=0):
    pad = " " * indent
    inner = " " * (indent + 4)
    if isinstance(value, Obj):
        if not value:
            return "{}"
        parts = [inner + json.dumps(str(k)) + ": " + dump_json(v, indent + 4) for k, v in value]
        return "{\n" + ",\n".join(parts) + "\n" + pad + "}"
    if isinstance(value, list):
        if not value:
            return "[]"
        parts = [inner + dump_json(v, indent + 4) for v in value]
        return "[\n" + ",\n".join(parts) + "\n" + pad + "]"
    if isinstance(value, bool):
        return "true" if value else "false"
    if value is None:
        return "null"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return repr(value)
    return json.dumps(value, ensure_ascii=False)


def get(o, key, default=None):
    for k, v in o:
        if k == key:
            return v
    return default


def set_key(o, key, value):
    for i, (k, _) in enumerate(o):
        if k == key:
            o[i] = (k, value)
            return
    o.append((key, value))


def find_def(tui, key):
    defs = get(get(get(tui, "pageData"), "componentDefinitions"), "localComponentDefinitions")
    for d in defs:
        if get(d, "key") == key:
            return get(d, "value")
    return None


def parse_rect(s):
    return [float(x) for x in s.replace(",", " ").split()]


def rect_str(x, y, w, h):
    def n(v):
        return int(v) if float(v).is_integer() else v
    return "%s %s %s %s" % (n(x), n(y), n(w), n(h))


# ---------------------------------------------------------------------------
# drawing helpers
# ---------------------------------------------------------------------------
_font_cache = {}


def font(path, size):
    key = (path, size)
    if key not in _font_cache:
        _font_cache[key] = ImageFont.truetype(path, size)
    return _font_cache[key]


def rgba(hexstr):
    v = int(hexstr, 16)
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255, (v >> 24) & 255)


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def gradient(size, top, bottom):
    w, h = size
    img = Image.new("RGBA", size)
    d = ImageDraw.Draw(img)
    for y in range(h):
        d.line([(0, y), (w, y)], fill=lerp(top, bottom, y / max(1, h - 1)))
    return img


def rounded_gradient(size, radius, top, bottom, border=None, border_w=1):
    w, h = size
    base = gradient(size, top, bottom)
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, fill=255)
    out = Image.new("RGBA", size, (0, 0, 0, 0))
    out.paste(base, (0, 0), mask)
    if border:
        d = ImageDraw.Draw(out)
        d.rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, outline=border, width=border_w)
    return out


def draw_text_spaced(draw, xy, text, fnt, fill, spacing=0, anchor=None):
    if not spacing:
        draw.text(xy, text, font=fnt, fill=fill, anchor=anchor)
        return
    # approximate letter spacing; anchor ignored except "la"
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=fnt, fill=fill)
        x += draw.textlength(ch, font=fnt) + spacing


def text_width(draw, text, fnt, spacing=0):
    if not spacing:
        return draw.textlength(text, font=fnt)
    return sum(draw.textlength(ch, font=fnt) for ch in text) + spacing * max(0, len(text) - 1)


def draw_screw(d, cx, cy, r):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(74, 70, 64, 255), outline=(20, 19, 18, 255))
    d.ellipse([cx - r + 1, cy - r + 1, cx + r - 1, cy + r - 1], fill=(112, 107, 99, 255))
    d.line([(cx - r + 3, cy), (cx + r - 3, cy)], fill=(28, 27, 25, 255), width=1)


def waveform(d, x, y, w, h, kind, colour, width=2):
    if kind == "saw":
        pts = [(x, y + h), (x + w * .5, y), (x + w * .5, y + h), (x + w, y)]
    elif kind == "triangle":
        pts = [(x, y + h), (x + w * .5, y), (x + w, y + h)]
    elif kind == "square":
        pts = [(x, y + h), (x, y), (x + w * .5, y), (x + w * .5, y + h), (x + w, y + h), (x + w, y)]
    elif kind == "pulse":
        pts = [(x, y + h), (x, y), (x + w * .25, y), (x + w * .25, y + h), (x + w, y + h)]
    elif kind == "sine":
        pts = [(x + i * w / 24, y + h / 2 - math.sin(i / 24 * 2 * math.pi) * h * .45) for i in range(25)]
    elif kind == "sh":
        pts = [(x, y + h * .8)] + [(x + w * i / 5, y + h * (.12 + .6 * ((i * 37) % 5) / 5)) for i in range(1, 6)] + [(x + w, y + h * .5)]
    else:
        return
    d.line(pts, fill=colour, width=width, joint="curve")


def draw_icon(d, kind, x, y, w, h):
    c = (154, 146, 132, 255)
    dim = (94, 89, 82, 255)
    if kind == "vco":
        waveform(d, x, y + 3, w * .28, h - 8, "triangle", dim, 2)
        waveform(d, x + w * .34, y, w * .28, h - 3, "saw", c, 2)
        waveform(d, x + w * .68, y + 3, w * .28, h - 8, "square", dim, 2)
    elif kind == "mixer":
        for i, hh in enumerate((.45, .75, .58)):
            xx = x + w * (0.18 + i * .32)
            d.line([(xx, y + h - h * hh), (xx, y + h)], fill=c, width=3)
            d.ellipse([xx - 3, y + h - h * hh - 5, xx + 3, y + h - h * hh + 1], fill=c)
    elif kind == "master":
        d.ellipse([x + w * .35, y, x + w * .65, y + h * .3], fill=(202, 58, 42, 255))
        d.ellipse([x + w * .43, y + h * .08, x + w * .57, y + h * .22], fill=(255, 128, 88, 255))
        d.line([(x, y + h * .72), (x + w, y + h * .72)], fill=c, width=2)
        for i in range(5):
            xx = x + i * w / 4
            d.line([(xx, y + h * .62), (xx, y + h * .82)], fill=dim, width=1)
    elif kind == "filter":
        pts = [(x, y + 3), (x + w * .5, y + 3), (x + w * .62, y + h * .25), (x + w * .72, y + h * .7), (x + w, y + h * .88)]
        d.line(pts, fill=c, width=2, joint="curve")
        d.line([(x, y + h - 2), (x + w, y + h - 2)], fill=dim, width=1)
    elif kind == "adsr":
        pts = [(x, y + h - 2), (x + w * .16, y + 2), (x + w * .38, y + h * .55), (x + w * .66, y + h * .55), (x + w, y + h - 2)]
        d.line(pts, fill=c, width=2, joint="curve")
        for px, py in pts[1:4]:
            d.ellipse([px - 2, py - 2, px + 2, py + 2], fill=(202, 58, 42, 255))
    elif kind == "mode":
        d.line([(x, y + h - 2), (x + w * .45, y + h - 2), (x + w * .55, y + 3), (x + w, y + 3)], fill=c, width=2, joint="curve")
        d.line([(x, y + 3), (x + w * .35, y + 3), (x + w * .45, y + h - 2)], fill=dim, width=2, joint="curve")
    elif kind == "out":
        d.line([(x, y + h / 2), (x + w * .7, y + h / 2)], fill=c, width=2)
        d.polygon([(x + w * .7, y + h * .25), (x + w, y + h / 2), (x + w * .7, y + h * .75)], fill=c)
    elif kind == "vca":
        d.polygon([(x + w * .5, y), (x + w, y + h), (x, y + h)], outline=c, width=2)
        d.line([(x + w * .5, y + h), (x + w * .5, y + h * .55)], fill=(202, 58, 42, 255), width=2)
    elif kind == "voice":
        for i, xx in enumerate((.25, .55, .4)):
            d.ellipse([x + w * xx - 4, y + h * .2, x + w * xx + 4, y + h * .2 + 8], fill=c)
        d.line([(x + w * .3, y + h * .7), (x + w * .7, y + h * .7)], fill=c, width=2)
        d.line([(x + w * .5, y + h * .45), (x + w * .5, y + h * .85)], fill=c, width=2)
    elif kind == "extras":
        for i in range(3):
            xx = x + w * (.18 + i * .3)
            d.ellipse([xx - 4, y + h * .25, xx + 4, y + h * .25 + 8], outline=c, width=2)
            d.line([(xx, y + h * .25 + 8), (xx, y + h * .8)], fill=dim, width=1)
    elif kind == "lfo":
        waveform(d, x, y + 2, w, h - 4, "sine", c, 2)
        d.line([(x, y + h - 2), (x + w, y + h - 2)], fill=dim, width=1)
    elif kind == "routing":
        for i in range(3):
            yy = y + h * (.25 + i * .25)
            d.line([(x, yy), (x + w * .55, yy)], fill=c, width=2)
            d.polygon([(x + w * .55, yy - 3), (x + w * .65, yy), (x + w * .55, yy + 3)], fill=c)
        d.line([(x + w * .75, y), (x + w * .75, y + h)], fill=dim, width=2)
    elif kind == "tune":
        d.line([(x + w * .5, y), (x + w * .5, y + h)], fill=c, width=2)
        d.line([(x + w * .25, y + h * .15), (x + w * .75, y + h * .15)], fill=c, width=2)
        d.arc([x + w * .15, y + h * .55, x + w * .85, y + h * 1.1], 200, 340, fill=c, width=2)


# ---------------------------------------------------------------------------
# panel background
# ---------------------------------------------------------------------------
def make_background(page, index):
    W, H = 1280, 628
    img = Image.new("RGBA", (W * S, H * S), (18, 17, 16, 255))

    if np is not None:
        base = np.array(gradient((W * S, H * S), (26, 24, 22, 255), (12, 11, 10, 255)).convert("RGB")).astype(np.int16)
        rng = np.random.default_rng(1982 + index)
        noise = rng.normal(0, 3.0, (H * S, W * S, 1))
        base = np.clip(base + noise, 0, 255).astype(np.uint8)
        img = Image.fromarray(base, "RGB").convert("RGBA")

    d = ImageDraw.Draw(img)

    # aluminium edge / case frame
    d.rectangle([0, 0, W * S - 1, H * S - 1], outline=(92, 87, 79, 255), width=2 * S)
    d.rectangle([2 * S, 2 * S, W * S - 2 * S, H * S - 2 * S], outline=(40, 38, 35, 255), width=1 * S)
    d.rectangle([7 * S, 7 * S, W * S - 7 * S, H * S - 7 * S], outline=(58, 54, 49, 255), width=1 * S)

    # header
    x = 24 * S
    y = 7 * S
    big = font(FONT_BOLD, 27 * S)
    small = font(FONT_REG, 8 * S)
    draw_text_spaced(d, (x, y), "ПОЛИВОКС", big, (239, 232, 218, 255), spacing=1.5 * S)
    tw = text_width(d, "ПОЛИВОКС", big, 1.5 * S)
    d.text((x + tw + 9 * S, y + 1 * S), "1982", font=font(FONT_BOLD, 23 * S), fill=(202, 58, 42, 255))
    d.text((x, y + 30 * S), "DUOPHONIC SYNTHESIZER  ·  MPC EDITION", font=small, fill=(145, 137, 124, 255))

    # title at right
    ru = font(FONT_BOLD, 14 * S)
    en = font(FONT_REG, 8 * S)
    d.text((W * S - 24 * S, y + 2 * S), page["ru"], font=ru, fill=(231, 224, 211, 255), anchor="ra")
    d.text((W * S - 24 * S, y + 23 * S), page["en"], font=en, fill=(145, 137, 124, 255), anchor="ra")

    # red line under header
    d.line([(24 * S, 39 * S), (W * S - 24 * S, 39 * S)], fill=(150, 45, 34, 255), width=1 * S)
    d.line([(24 * S, 40 * S), (W * S - 24 * S, 40 * S)], fill=(55, 51, 46, 255), width=1 * S)

    # four raised module plates
    plate_top = (49, 46, 42, 255)
    plate_bottom = (31, 29, 27, 255)
    for i, (ru_name, en_name, icon) in enumerate(page["modules"]):
        px = SLOT_X[i] - 10
        pw = SLOT_W + 20
        py = PLATE_Y
        ph = PLATE_H
        shadow = Image.new("RGBA", (pw * S, ph * S), (0, 0, 0, 0))
        ds = ImageDraw.Draw(shadow)
        ds.rounded_rectangle([2 * S, 3 * S, (pw - 1) * S, (ph - 1) * S], radius=5 * S, fill=(0, 0, 0, 120))
        img.alpha_composite(shadow, (px * S, py * S))
        plate = rounded_gradient((pw * S, ph * S), 5 * S, plate_top, plate_bottom,
                                 border=(92, 86, 78, 255), border_w=1 * S)
        # inner highlight
        dp = ImageDraw.Draw(plate)
        dp.rounded_rectangle([1 * S, 1 * S, (pw - 2) * S, (ph - 2) * S], radius=4 * S,
                             outline=(64, 59, 53, 255), width=1 * S)
        dp.line([(5 * S, 2 * S), ((pw - 5) * S, 2 * S)], fill=(118, 110, 99, 90), width=1 * S)
        img.alpha_composite(plate, (px * S, py * S))

        # screws
        for sx, sy in ((px + 10, py + 10), (px + pw - 10, py + 10),
                       (px + 10, py + ph - 10), (px + pw - 10, py + ph - 10)):
            draw_screw(d, sx * S, sy * S, 4 * S)

        # module title + subtitle. Leave room for the MPC's Q-Link badge,
        # which is drawn at the top-left of the active column.
        tx = (px + 40) * S
        ty = (py + 9) * S
        f_ru = font(FONT_BOLD, 17 * S)
        f_en = font(FONT_REG, 8 * S)
        d.text((tx, ty), ru_name, font=f_ru, fill=(236, 229, 216, 255))
        d.text((tx, ty + 25 * S), en_name.upper(), font=f_en, fill=(150, 142, 129, 255))
        draw_icon(d, icon, (px + pw - 47) * S, (py + 11) * S, 34 * S, 27 * S)

        # thin rule under the module header
        d.line([((px + 11) * S, (py + 45) * S), ((px + pw - 11) * S, (py + 45) * S)],
               fill=(70, 65, 59, 255), width=1 * S)

    return img.resize((W, H), Image.LANCZOS).convert("RGBA")


# ---------------------------------------------------------------------------
# knob film strip
# ---------------------------------------------------------------------------
def make_knob_strip(bipolar, out_path, frames=127, size=90):
    f = size * 4
    strips = []
    for i in range(frames):
        t = i / (frames - 1)
        ang = math.radians(-135 + 270 * t)
        frame = Image.new("RGBA", (f, f), (0, 0, 0, 0))
        d = ImageDraw.Draw(frame)
        cx = cy = f / 2
        # scale ring / ticks
        for k in range(11):
            a = math.radians(-135 + k * 27)
            r0, r1 = f * .415, f * .465
            d.line([(cx + math.sin(a) * r0, cy - math.cos(a) * r0),
                    (cx + math.sin(a) * r1, cy - math.cos(a) * r1)],
                   fill=(135, 127, 114, 210), width=max(1, int(f * .008)))
        # unlit arc
        def arc_points(a0, a1, radius):
            pts = []
            n = max(2, int(abs(a1 - a0) / 3))
            for j in range(n + 1):
                a = math.radians(a0 + (a1 - a0) * j / n)
                pts.append((cx + math.sin(a) * radius, cy - math.cos(a) * radius))
            return pts
        d.line(arc_points(-135, 135, f * .407), fill=(61, 57, 51, 255), width=max(2, int(f * .012)))
        a0 = 0 if bipolar else -135
        a1 = math.degrees(ang)
        if abs(a1 - a0) > 0.2:
            d.line(arc_points(a0, a1, f * .407),
                   fill=(205, 83, 52, 255), width=max(2, int(f * .012)))
        # outer silver ring
        d.ellipse([cx - f * .375, cy - f * .375, cx + f * .375, cy + f * .375],
                  fill=(63, 59, 53, 255), outline=(151, 142, 127, 255), width=max(1, int(f * .008)))
        # body
        d.ellipse([cx - f * .34, cy - f * .34, cx + f * .34, cy + f * .34], fill=(24, 23, 21, 255))
        # radial-ish highlight: concentric translucent ellipses
        for j in range(18):
            r = f * (.33 - j * .015)
            col = (74 - j * 2, 70 - j * 2, 64 - j * 2, 255)
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=col)
        # inner shadow ring
        d.ellipse([cx - f * .31, cy - f * .31, cx + f * .31, cy + f * .31],
                  outline=(10, 10, 9, 255), width=max(2, int(f * .015)))
        # pointer
        p0 = (cx + math.sin(ang) * f * .09, cy - math.cos(ang) * f * .09)
        p1 = (cx + math.sin(ang) * f * .295, cy - math.cos(ang) * f * .295)
        d.line([p0, p1], fill=(244, 236, 220, 255), width=max(3, int(f * .032)))
        d.ellipse([p1[0] - f * .026, p1[1] - f * .026, p1[0] + f * .026, p1[1] + f * .026],
                  fill=(240, 229, 210, 255))
        # centre cap
        r = f * .075
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(17, 16, 15, 255),
                  outline=(105, 97, 86, 255), width=max(1, int(f * .008)))
        strips.append(frame.resize((size, size), Image.LANCZOS))

    out = Image.new("RGBA", (size, size * frames), (0, 0, 0, 0))
    for i, frame in enumerate(strips):
        out.paste(frame, (0, i * size), frame)
    out.save(out_path)


# ---------------------------------------------------------------------------
# switch artwork
# ---------------------------------------------------------------------------
def make_toggle(on, out_path):
    f = 4
    w, h = 84, 68
    img = Image.new("RGBA", (w * f, h * f), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([1 * f, 1 * f, (w - 1) * f, (h - 1) * f], radius=8 * f, fill=(17, 16, 15, 255),
                        outline=(86, 80, 71, 255), width=1 * f)
    d.rounded_rectangle([5 * f, 5 * f, (w - 5) * f, (h - 5) * f], radius=6 * f, fill=(11, 11, 10, 255))
    if on:
        py = 7 * f
        face = (238, 232, 220, 255)
        edge = (150, 144, 132, 255)
    else:
        py = 35 * f
        face = (168, 162, 150, 255)
        edge = (84, 80, 72, 255)
    d.rounded_rectangle([17 * f, py, 67 * f, py + 26 * f], radius=4 * f, fill=edge)
    d.rounded_rectangle([18 * f, py + 2 * f, 66 * f, py + 24 * f], radius=3 * f, fill=face)
    # grip lines
    for yy in (py + 8 * f, py + 13 * f, py + 18 * f):
        d.line([(24 * f, yy), (60 * f, yy)], fill=(110, 104, 94, 110), width=1 * f)
    # LED
    ly = 17 * f if on else 52 * f
    col = (222, 70, 50, 255) if on else (58, 30, 26, 255)
    d.ellipse([69 * f, ly - 3 * f, 76 * f, ly + 4 * f], fill=col)
    if on:
        d.ellipse([71 * f, ly - 1 * f, 74 * f, ly + 2 * f], fill=(255, 188, 160, 255))
    img.resize((w, h), Image.LANCZOS).save(out_path)


# ---------------------------------------------------------------------------
# TUI patching
# ---------------------------------------------------------------------------
def patch_tui(tui, out_dir):
    page_data = get(tui, "pageData")
    tabs = get(page_data, "tabs")
    for i, tab in enumerate(tabs):
        root = get(tab, "componentDefinition")
        bg = get(root, "backgroundData")
        for which in ("focussed", "unfocussed"):
            node = get(bg, which)
            set_key(node, "colour", "ff121110")
            set_key(node, "image", "bg_%s.png" % PAGES[i]["name"].lower())
        set_key(tab, "qlinkBoundsData", [rect_str(SLOT_X[c] - 10, PLATE_Y, SLOT_W + 20, PLATE_H) for c in range(4)])

        components = get(root, "componentsData")
        for entry in components:
            bounds = get(entry, "bounds")
            x, y, w, h = parse_rect(get(bounds, "bounds"))
            col = min(range(4), key=lambda c: abs(x - SLOT_X[c]))
            row = min(range(4), key=lambda r: abs(y - SLOT_Y[r]))
            set_key(bounds, "bounds", rect_str(SLOT_X[col], NEW_Y[row], SLOT_W, SLOT_H))
            cd = get(entry, "componentData")
            if get(cd, "type") == "radio2":
                # A two-position paddle with a live value label reads better
                # than two unlabelled segment buttons.
                set_key(cd, "type", "toggle")

    # Make the dynamic labels a little smaller, to fit long Polivoks names.
    defs = get(get(page_data, "componentDefinitions"), "localComponentDefinitions")
    # Two-position switches were changed to the single-paddle "toggle"
    # component above, so the unlabelled radio2 definition is now dead weight
    # (and would reference the unused segment artwork).
    defs[:] = [d for d in defs if get(d, "key") != "radio2"]
    for d in defs:
        key = get(d, "key")
        comp = get(d, "value")
        if not (key.startswith("knob") or key in ("toggle", "radio2")):
            continue
        for child in get(comp, "componentsData"):
            cd = get(child, "componentData")
            name = get(cd, "name")
            typ = get(cd, "type")
            data = get(cd, "data")
            if typ != "Label":
                continue
            style = get(data, "textStyle")
            fnt = get(style, "font")
            b = get(child, "bounds")
            if key.startswith("knob"):
                if name == "name":
                    set_key(fnt, "height", 22.0)
                    set_key(b, "bounds", "110 10 158 34")
                elif name == "value":
                    set_key(fnt, "height", 27.0)
                    set_key(b, "bounds", "110 46 158 42")
            elif key == "toggle":
                if name == "name":
                    set_key(fnt, "height", 22.0)
                    set_key(b, "bounds", "15 9 160 34")
                elif name == "value":
                    set_key(fnt, "height", 26.0)
                    set_key(b, "bounds", "15 45 160 36")
            elif key == "radio2":
                if name == "name":
                    set_key(fnt, "height", 22.0)
                    set_key(b, "bounds", "15 8 248 34")

    # Make the overlay panels match the case.
    for key, comp in [(k, get(d, "value")) for d in defs for k in [get(d, "key")]]:
        if key in ("knob overlay", "knob overlay bipolar", "menu overlay"):
            bg = get(comp, "backgroundData")
            for which in ("focussed", "unfocussed"):
                node = get(bg, which)
                set_key(node, "colour", "ff121110")


# ---------------------------------------------------------------------------
# preview renderer (approximate, for local inspection)
# ---------------------------------------------------------------------------
def render_preview(tui, params, out_dir, asset_dir):
    page_data = get(tui, "pageData")
    defs = {get(d, "key"): get(d, "value") for d in get(get(page_data, "componentDefinitions"), "localComponentDefinitions")}
    pdata = {p["index"]: p for p in params["params"]}
    tabs = get(page_data, "tabs")
    images = []
    for i, tab in enumerate(tabs):
        root = get(tab, "componentDefinition")
        bg_name = get(get(get(root, "backgroundData"), "unfocussed"), "image")
        img = Image.open(os.path.join(out_dir, bg_name)).convert("RGBA")
        draw = ImageDraw.Draw(img)
        for entry in get(root, "componentsData"):
            cd = get(entry, "componentData")
            typ = get(cd, "type")
            if typ not in defs:
                continue
            remap = get(get(entry, "handle remapping"), "map")
            pidx = None
            for kv in remap:
                if get(kv, "key") == "Data":
                    val = get(kv, "value")
                    if val.startswith("Parameter "):
                        pidx = int(val.split()[1])
            if pidx is None:
                continue
            bx, by, bw, bh = parse_rect(get(get(entry, "bounds"), "bounds"))
            render_def(defs[typ], bx, by, pidx, pdata, img, draw, defs, asset_dir)
        images.append(img)
        img.save(os.path.join(out_dir, "preview_%s.png" % PAGES[i]["name"].lower()))

    # 2x2 contact sheet with a thin gap
    sheet = Image.new("RGBA", (1280 * 2 + 8, 628 * 2 + 8), (40, 38, 35, 255))
    for i, im in enumerate(images):
        sheet.paste(im, ((i % 2) * 1288, (i // 2) * 636))
    sheet.save(os.path.join(out_dir, "preview_all.png"))


def render_def(comp, ox, oy, pidx, pdata, img, draw, defs, asset_dir):
    for child in get(comp, "componentsData"):
        cd = get(child, "componentData")
        typ = get(cd, "type")
        data = get(cd, "data")
        bx, by, bw, bh = parse_rect(get(get(child, "bounds"), "bounds"))
        x, y = ox + bx, oy + by
        handle = get(data, "handleName", "Data")
        local = pidx
        if typ == "Knob":
            strip = get(data, "filmStrip")
            frames = int(get(data, "numFrames", 127))
            p = pdata.get(local)
            if not p or not strip:
                continue
            norm = float(p.get("default", 0.5))
            frame = max(0, min(frames - 1, round(norm * (frames - 1))))
            path = os.path.join(asset_dir, strip)
            im = Image.open(path).convert("RGBA")
            fh = im.height // frames
            crop = im.crop((0, frame * fh, im.width, (frame + 1) * fh)).resize((int(bw), int(bh)), Image.LANCZOS)
            img.alpha_composite(crop, (int(x), int(y)))
        elif typ == "Label":
            p = pdata.get(local)
            label_type = get(data, "type")
            if label_type == "Name":
                text = p["name"] if p else ""
            elif label_type == "Value":
                text = p.get("display", "") if p else ""
            else:
                text = get(data, "text", "")
            style = get(data, "textStyle")
            fnt = get(style, "font")
            colour = rgba(get(style, "colour"))
            just = get(style, "justification", "left")
            size = int(get(fnt, "height", 24))
            f = font(FONT_REG, size)
            if get(style, "case") == "Upper Case":
                text = text.upper()
            if "right" in just:
                draw.text((x + bw, y + bh / 2), text, font=f, fill=colour, anchor="rm")
            elif "horizontallyCentred" in just:
                draw.text((x + bw / 2, y + bh / 2), text, font=f, fill=colour, anchor="mm")
            else:
                draw.text((x, y + bh / 2), text, font=f, fill=colour, anchor="lm")
        elif typ == "Button":
            p = pdata.get(local)
            if not p:
                continue
            norm = float(p.get("default", 0))
            choices = p.get("choices") or []
            if len(choices) > 1:
                step = round(norm * (len(choices) - 1))
            else:
                step = 1 if norm > 0.5 else 0
            on = step == int(get(data, "buttonId", 0))
            path = os.path.join(asset_dir, get(data, "onImage" if on else "offImage"))
            im = Image.open(path).convert("RGBA")
            img.alpha_composite(im, (int(x), int(y)))
        elif typ == "Focus":
            continue
        elif typ in ("KnobOverlay", "MenuOverlay"):
            continue
        else:
            # nested custom component
            nested = defs.get(typ)
            if nested:
                render_def(nested, x, y, local, pdata, img, draw, defs, asset_dir)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sdk", default=os.path.join(ROOT, ".sdk"))
    ap.add_argument("--params", default=None)
    ap.add_argument("--out", default=SKIN)
    ap.add_argument("--preview", action="store_true")
    args = ap.parse_args()

    params_path = args.params
    if not params_path:
        params_path = os.path.join(args.sdk, "plugins", "Polivoks 1982", "build", "params.json")
    if not os.path.isfile(params_path):
        sys.exit("params.json not found: %s\nBuild the plugin first, or pass --params." % params_path)
    if not os.path.isfile(os.path.join(args.sdk, "tools", "gen_ui.py")):
        sys.exit("MPC Plugin SDK not found at %s (pass --sdk)." % args.sdk)

    os.makedirs(args.out, exist_ok=True)
    for name in os.listdir(args.out):
        if name.endswith(".png") or name.endswith(".json"):
            os.remove(os.path.join(args.out, name))

    tmp = tempfile.mkdtemp(prefix="polivoks-skin-")
    try:
        cmd = [sys.executable, os.path.join(args.sdk, "tools", "gen_ui.py"),
               "--params", params_path, "--ui", UI, "--out", tmp,
               "--kit-cache", os.path.join(tmp, "kit")]
        print("+", " ".join(cmd))
        subprocess.run(cmd, check=True)
        shutil.copy(os.path.join(tmp, "Q-Links.json"), args.out)

        with open(os.path.join(tmp, "TUI.json"), "r", encoding="utf-8") as fh:
            tui = json.load(fh, object_pairs_hook=lambda pairs: Obj(pairs))
        patch_tui(tui, args.out)
        with open(os.path.join(args.out, "TUI.json"), "w", encoding="utf-8") as fh:
            fh.write(dump_json(tui))
            fh.write("\n")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    # Panel artwork
    for i, page in enumerate(PAGES):
        make_background(page, i).save(os.path.join(args.out, "bg_%s.png" % page["name"].lower()))
    make_knob_strip(False, os.path.join(args.out, "knob_uni.png"))
    make_knob_strip(True, os.path.join(args.out, "knob_bi.png"))
    make_toggle(False, os.path.join(args.out, "toggle_off.png"))
    make_toggle(True, os.path.join(args.out, "toggle_on.png"))

    # Prune kit files that this skin does not use.
    for name in os.listdir(args.out):
        if name.startswith(("fader_", "button_", "knob_small_", "seg2_", "seg3_", "seg4_")) or name == "kit.json":
            os.remove(os.path.join(args.out, name))

    if args.preview:
        with open(params_path, "r", encoding="utf-8") as fh:
            params = json.load(fh)
        render_preview(tui, params, args.out, args.out)
        # Keep previews out of the deployable skin folder.
        prev_dir = os.path.join(PLUGIN, "build", "skin-preview")
        os.makedirs(prev_dir, exist_ok=True)
        for name in os.listdir(args.out):
            if name.startswith("preview_"):
                shutil.move(os.path.join(args.out, name), os.path.join(prev_dir, name))
        print("previews:", prev_dir)

    print("skin:", args.out)


if __name__ == "__main__":
    main()
