#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 SEN Labs e.U.
"""
The application icon of Shoji: the knot of the logo as filled shapes, in two drawings (detail levels of one vector icon): the full one (from 32 px up) and a
plain one with darker outlines for 16 and 24 px.

    shoji-icon.py svg                      > shoji-icon.svg      the two drawings, the plain one after the full one
    shoji-icon.py lod in.hvif out.hvif N   N = number of shapes of the full drawing (icon2icon made in.hvif from the svg): sets the detail levels
    shoji-icon.py rdef icon.hvif           > the resource for Resources.rdef

Steps (icon2icon is on Haiku):  svg -> icon2icon shoji-icon.svg shoji-icon.hvif -> lod -> rdef.
"""
import sys

LOBES = [(32, 18), (18, 32), (46, 32), (32, 46)]
SMALL_LOBES = [(32, 19), (19, 32), (45, 32), (32, 45)]
SPLIT = 0.455	# the scale (pixels / 64) below which the plain drawing is used: 16 and 24 px


def dia(r):
	return f'M32 {32 - r} L{32 + r} 32 L32 {32 + r} L{32 - r} 32 Z'


def full(o):
	for i, (x, y) in enumerate(LOBES):
		o.append(f'    <linearGradient id="wood{i}" gradientUnits="userSpaceOnUse" x1="{x}" y1="{y - 12}" x2="{x}" y2="{y + 12}"><stop offset="0" stop-color="#d9bc9f"/><stop offset="1" stop-color="#9e7353"/></linearGradient>')
		o.append(f'    <linearGradient id="paper{i}" gradientUnits="userSpaceOnUse" x1="{x}" y1="{y - 9.2}" x2="{x}" y2="{y + 9.2}"><stop offset="0" stop-color="#fcf8f0"/><stop offset="1" stop-color="#e5cbb0"/></linearGradient>')


def shapes_full(o):
	for i, (x, y) in enumerate(LOBES):
		o.append(f'  <circle cx="{x}" cy="{y}" r="12" fill="url(#wood{i})"/>')
	for i, (x, y) in enumerate(LOBES):
		o.append(f'  <circle cx="{x}" cy="{y}" r="9.2" fill="url(#paper{i})"/>')
	o.append(f'  <path d="{dia(22)}" fill="#9e7353"/>')
	o.append(f'  <path d="{dia(20)}" fill="#f2eadc"/>')
	o.append(f'  <path d="{dia(16)}" fill="#9e7353"/>')
	o.append(f'  <path d="{dia(14.4)}" fill="#f2eadc"/>')
	o.append(f'  <path d="{dia(7.5)}" fill="#9e7353"/>')
	o.append(f'  <path d="{dia(5)}" fill="#1c2535"/>')
	for x, y in LOBES:
		o.append(f'  <circle cx="{x}" cy="{y}" r="2.4" fill="#121925"/>')
	o.append('  <rect x="31.6" y="5" width="0.8" height="54" fill="#6b4c33" fill-opacity="0.75"/>')


def shapes_small(o):
	# dark outline, wood, paper; the middle dark: few shapes, strong contrast
	for x, y in SMALL_LOBES:
		o.append(f'  <circle cx="{x}" cy="{y}" r="13.5" fill="#4a3522"/>')
	for x, y in SMALL_LOBES:
		o.append(f'  <circle cx="{x}" cy="{y}" r="10.5" fill="#c69c77"/>')
	for x, y in SMALL_LOBES:
		o.append(f'  <circle cx="{x}" cy="{y}" r="6.5" fill="#fcf8f0"/>')
	o.append(f'  <path d="{dia(13)}" fill="#4a3522"/>')
	o.append(f'  <path d="{dia(9.5)}" fill="#c69c77"/>')
	o.append(f'  <path d="{dia(6.3)}" fill="#1c2535"/>')


def svg():
	o = ['<?xml version="1.0" encoding="UTF-8"?>',
		'<!-- Shoji application icon: the knot of the logo, filled shapes for HVIF (64x64), made by shoji-icon.py. The first shapes are the full drawing, the last ones the plain drawing for 16 and 24 px -->',
		'<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">', '  <defs>']
	full(o)
	o.append('  </defs>')
	shapes_full(o)
	shapes_small(o)
	o.append('</svg>')
	return '\n'.join(o) + '\n'


# --- a reader for the shapes of an HVIF (to set the detail levels) ---

def skip_coord(b, p):
	return p + (2 if b[p] & 0x80 else 1)


def read_shapes_offset(b):
	assert b[:4] == b'ncif', 'not an HVIF'
	p = 4
	n = b[p]; p += 1
	for _ in range(n):			# styles
		t = b[p]; p += 1
		if t == 1: p += 4
		elif t == 3: p += 3
		elif t == 4: p += 2
		elif t == 5: p += 1
		elif t == 2:
			gtype, flags, stops = b[p], b[p + 1], b[p + 2]; p += 3
			if flags & 2: p += 18
			per = 3 if flags & 4 else (1 if flags & 16 else 4)
			if flags & 16 and not flags & 4: per = 2
			p += stops * (1 + per)
		else:
			raise ValueError(f'style type {t}')
	n = b[p]; p += 1
	for _ in range(n):			# paths
		flags = b[p]; points = b[p + 1]; p += 2
		if flags & 4:
			cmds = b[p:p + (points + 3) // 4]; p += len(cmds)
			for i in range(points):
				c = (cmds[i // 4] >> ((i % 4) * 2)) & 3
				for _ in range((1, 1, 2, 6)[c]):
					p = skip_coord(b, p)
		else:
			for _ in range(points * (2 if flags & 8 else 6)):
				p = skip_coord(b, p)
	return p


def lod(data, nfull):
	b = bytearray(data)
	p = read_shapes_offset(b)
	count = b[p]; p += 1
	out = bytearray(b[:p])
	assert nfull < count
	small_max = round(SPLIT * 63.75)
	for i in range(count):
		assert b[p] == 0x0a, f'shape {i} type {b[p]}'
		npaths = b[p + 2]
		end = p + 3 + npaths
		flags = b[end]
		assert flags == 0, 'shape flags are expected to be 0'
		out += b[p:end]
		out.append(0x08)
		out += bytes((small_max, 255)) if i < nfull else bytes((0, small_max))
		p = end + 1
	assert p == len(b), f'{len(b) - p} bytes left after the shapes'
	return bytes(out)


def rdef(data):
	h = data.hex().upper()
	lines = ['\t$"' + h[i:i + 64] + '"' for i in range(0, len(h), 64)]
	return 'resource vector_icon {\n' + '\n'.join(lines) + '\n};\n'


if __name__ == '__main__':
	cmd = sys.argv[1] if len(sys.argv) > 1 else ''
	if cmd == 'svg':
		sys.stdout.write(svg())
	elif cmd == 'lod' and len(sys.argv) == 5:
		open(sys.argv[3], 'wb').write(lod(open(sys.argv[2], 'rb').read(), int(sys.argv[4])))
	elif cmd == 'rdef' and len(sys.argv) == 3:
		sys.stdout.write(rdef(open(sys.argv[2], 'rb').read()))
	else:
		sys.exit(__doc__)
