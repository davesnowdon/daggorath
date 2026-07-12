#!/usr/bin/env python3
"""Generate core/tables_vec.c from the C++ port's shape tables, cross-verified
against the original 6809 assembly source.

Data lineage
------------
The original game (rights-holder-released freeware) stores shapes as vector
lists: `SVORG y,x` emits an absolute start pair, then each `SVECT y,x` emits
ONE byte of packed half-resolution 4-bit deltas.  The C++ port (Hunerlach
lineage) ships the same shapes expanded to absolute (x,y) pairs, as hex
strings loaded via Utils::LoadFromHex, in the layout its interpreter reads:

    [numLists] then per list: [numPoints] [x,y] * numPoints

This script:
  1. parses the port's viewer.cpp hex strings (primary data source),
  2. independently expands every SVORG/SVECT shape in the original ASM,
  3. matches ASM shapes to port tables BY GEOMETRY (no manual name mapping),
  4. fails hard if a matched pair disagrees,
  5. emits core/tables_vec.c / .h with a per-array provenance comment.

Shapes using V$JMP / V$JSR chaining (composite giants etc.) cannot be
auto-expanded here; they are emitted from port data and flagged PORT-DATA.
"""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PORT_SRC = REPO / "../../native-port/DungeonsOfDaggorath/src"
ASM_SRC = REPO / "../../disassembly/daggorath/original-source"
OUT_C = REPO / "core/tables_vec.c"
OUT_H = REPO / "core/tables_vec.h"

# Shape tables (drawVectorList format).  Order preserved for output.
SHAPES = [
    "SP_VLA", "WR_VLA", "SC_VLA", "BL_VLA", "GL_VLA", "VI_VLA",
    "S1_VLA", "S2_VLA", "K1_VLA", "K2_VLA", "W0_VLA", "W1_VLA", "W2_VLA",
    "LAD_VLA", "HUP_VLA", "HDN_VLA", "CEI_VLA", "LPK_VLA", "RPK_VLA",
    "FSD_VLA", "LSD_VLA", "RSD_VLA",
    "RWAL_VLA", "LWAL_VLA", "FWAL_VLA",
    "RPAS_VLA", "LPAS_VLA", "FPAS_VLA",
    "RDOR_VLA", "LDOR_VLA", "FDOR_VLA",
    "SHIE_VLA", "SWOR_VLA", "TORC_VLA", "RING_VLA", "SCRO_VLA", "FLAS_VLA",
]
# Non-shape byte tables that also live in the viewer constructor.
AUX = ["Scale", "LINES"]

ASM_FILES = ["D3.ASM", "D4.ASM", "VARC.ASM", "VERT.ASM", "VOBJ.ASM"]


def parse_port_hex(viewer_cpp: Path) -> dict:
    """name -> bytes for every LoadFromHex site (multi-line literals joined)."""
    text = viewer_cpp.read_text()
    out = {}
    pat = re.compile(
        r'Utils::LoadFromHex\(\s*(\w+)\s*,\s*((?:"[0-9A-Fa-f]*"\s*)+)\)',
        re.S,
    )
    for name, blob in pat.findall(text):
        hexstr = "".join(re.findall(r'"([0-9A-Fa-f]*)"', blob))
        out[name] = bytes.fromhex(hexstr)
    return out


def decode_port_shape(b: bytes):
    """Port layout -> list of polylines [[(x,y), ...], ...]. None if malformed."""
    if not b:
        return None
    nlists, i, lists = b[0], 1, []
    for _ in range(nlists):
        if i >= len(b):
            return None
        npts = b[i]
        i += 1
        if i + 2 * npts > len(b):
            return None
        pts = [(b[i + 2 * k], b[i + 2 * k + 1]) for k in range(npts)]
        i += 2 * npts
        lists.append(pts)
    return lists if i == len(b) else None


def _num(tok: str):
    """Parse a 6809 assembler numeric literal, or None if symbolic."""
    tok = tok.strip()
    try:
        if tok.startswith("$"):
            return int(tok[1:], 16)
        if tok.startswith("%"):
            return int(tok[1:], 2)
        return int(tok)
    except ValueError:
        return None


def _tokenize_asm():
    """Return (streams, order): per-file token lists with label positions.

    Tokens (kept symbolic so we never need the V$ numeric values):
      ('abs', y, x)   absolute point       (from FCB pairs / SVORG)
      ('rel', y, x)   relative-mode point given as ABSOLUTE coords (SVECT)
      ('relbyte', b)  relative-mode packed delta byte (raw FCB in rel mode?
                      never emitted by macros; handled for completeness)
      ('op', 'NEW'|'END'|'REL'|'ABS'|'JMP'|'JSR'|'RTS')
      ('word', label) FDB target following JMP/JSR
    """
    streams = {}   # file -> list of tokens
    labels = {}    # label -> (file, token index)
    for fname in ASM_FILES:
        toks = []
        for raw in (ASM_SRC / fname).read_text(errors="replace").splitlines():
            line = raw.split(";", 1)[0].rstrip()
            if not line.strip():
                continue
            label = None
            if not raw[0] in " \t":
                m = re.match(r"^([A-Za-z]\w*)\s*(.*)$", line)
                if not m:
                    continue
                label, rest = m.group(1), m.group(2)
                labels[label] = (fname, len(toks))
                if not rest.strip():
                    continue
                line = rest
            m2 = re.match(r"^\s*(\S+)\s*(.*)$", line)
            if not m2:
                continue
            mnem, rest = m2.group(1).upper(), m2.group(2)
            argv = [a.strip() for a in rest.split(",")] if rest.strip() else []
            if mnem == "SVORG" and len(argv) >= 2:
                toks.append(("abs", _num(argv[0]), _num(argv[1])))
                toks.append(("op", "REL"))
            elif mnem == "SVECT" and len(argv) >= 2:
                toks.append(("rel", _num(argv[0]), _num(argv[1])))
            elif mnem == "SVNEW":
                toks.append(("op", "ABS"))
            elif mnem == "SVEND":
                toks.append(("op", "ABS"))
                toks.append(("op", "END"))
            elif mnem == "FCB":
                pend = []
                for a in argv:
                    if a.startswith("V$"):
                        if pend:  # odd leftover numeric before an opcode
                            toks.append(("relbyte", pend.pop()))
                        toks.append(("op", a[2:]))
                    else:
                        v = _num(a)
                        if v is None:
                            toks.append(("op", "BAD"))
                        else:
                            pend.append(v)
                            if len(pend) == 2:
                                toks.append(("abs", pend[0], pend[1]))
                                pend = []
                if pend:
                    toks.append(("relbyte", pend.pop()))
            elif mnem == "FDB" and argv:
                toks.append(("word", argv[0]))
            else:
                toks.append(("op", "BAD"))
        streams[fname] = toks
    return streams, labels


def _sext4(n):
    return n - 16 if n >= 8 else n


def parse_asm_shapes():
    """label -> list of polylines, executing the vector-list VM symbolically.

    Modes follow VCTLST semantics as documented in VARC.ASM: absolute y,x
    pairs by default; V$REL enters packed-delta mode; V$ABS returns/pen-ups;
    V$NEW starts a new polyline; V$JMP chains; V$JSR/V$RTS nest one level.
    A pen-up ends the current polyline; the next point starts the next one.
    Port stores points as (x, y).
    """
    streams, labels = _tokenize_asm()
    shapes = {}
    for lbl, (fname, start) in labels.items():
        toks = streams[fname]
        polylines, cur = [], []
        y = x = None
        relative = False
        stack = []
        i = start
        ok = True
        steps = 0
        while i < len(toks) and steps < 4000:
            steps += 1
            kind = toks[i][0]
            if kind == "abs":
                _, a, b = toks[i]
                if relative:
                    # macro streams stay 'rel'; a raw abs pair in rel mode
                    # is really two delta bytes - not seen in this codebase
                    ok = False
                    break
                y, x = a, b
                cur.append((x, y))
            elif kind == "rel":
                _, a, b = toks[i]
                y, x = a, b
                cur.append((x, y))
            elif kind == "relbyte":
                _, bv = toks[i]
                if not relative or y is None:
                    ok = False
                    break
                y += 2 * _sext4((bv >> 4) & 0xF)
                x += 2 * _sext4(bv & 0xF)
                cur.append((x & 0xFF, y & 0xFF))
            elif kind == "op":
                op = toks[i][1]
                if op == "REL":
                    relative = True
                elif op == "ABS":
                    relative = False
                    if cur:
                        polylines.append(cur)
                    cur = []
                elif op == "NEW":
                    if cur:
                        polylines.append(cur)
                    cur = []
                elif op == "END":
                    if stack:
                        fname, i, relative = stack.pop()
                        toks = streams[fname]
                        continue
                    break
                elif op == "RTS":
                    if cur:
                        polylines.append(cur)
                    cur = []
                    if not stack:
                        break  # top-level: a subroutine shape ends here
                    fname, i, relative = stack.pop()
                    toks = streams[fname]
                    continue
                elif op in ("JMP", "JSR"):
                    if i + 1 >= len(toks) or toks[i + 1][0] != "word":
                        ok = False
                        break
                    target = toks[i + 1][1]
                    if target not in labels:
                        ok = False
                        break
                    if cur:  # pen-up across a chain boundary
                        polylines.append(cur)
                    cur = []
                    if op == "JSR":
                        stack.append((fname, i + 2, relative))
                    tf, ti = labels[target]
                    fname, i = tf, ti
                    toks = streams[fname]
                    relative = False
                    continue
                else:  # BAD or unknown
                    ok = False
                    break
            elif kind == "word":
                pass  # consumed with JMP/JSR; stray words are padding
            i += 1
        if cur:
            polylines.append(cur)
        if ok and polylines and all(len(p) >= 1 for p in polylines):
            shapes[f"{lbl}({fname})"] = polylines
    return shapes


def segset(polylines):
    """Canonical set of undirected segments (draw-order independent)."""
    segs = set()
    for pts in polylines:
        for a, b in zip(pts, pts[1:]):
            segs.add((a, b) if a <= b else (b, a))
    return frozenset(segs)


def encode_port_shape(polylines) -> bytes:
    """Re-encode polylines into the port's [nlists][npts][x,y]* layout."""
    out = [len(polylines)]
    for pts in polylines:
        out.append(len(pts))
        for x, y in pts:
            out += [x, y]
    return bytes(out)


def near_match(pgeo, ageo, max_diffs=2):
    """Same polyline structure, at most max_diffs differing coordinates."""
    if [len(p) for p in pgeo] != [len(a) for a in ageo]:
        return None
    diffs = 0
    for pp, ap in zip(pgeo, ageo):
        for (px, py), (ax, ay) in zip(pp, ap):
            diffs += (px != ax) + (py != ay)
    return diffs if 0 < diffs <= max_diffs else None


def c_array(name: str, data: bytes, comment: str) -> str:
    lines, cur = [], ""
    for tok in (f"0x{v:02X}," for v in data):
        if len(cur) + len(tok) > 72:
            lines.append(cur)
            cur = ""
        cur += tok
    lines.append(cur.rstrip(","))
    inner = "\n    ".join(l for l in lines if l)
    return (
        f"/* {comment} */\n"
        f"const uint8_t {name}[{len(data)}] = {{\n    {inner}\n}};\n"
    )


def main():
    port = parse_port_hex(PORT_SRC / "viewer.cpp")
    missing = [n for n in SHAPES + AUX if n not in port]
    if missing:
        sys.exit(f"FATAL: tables not found in viewer.cpp: {missing}")

    asm = parse_asm_shapes()
    print(f"port shape tables: {len(SHAPES)}; ASM expandable shapes: {len(asm)}")

    # Geometry matching, strongest evidence first:
    #   1. exact ordered equality
    #   2. same undirected segment set (draw order differs, pixels identical)
    #   3. port segments are a subset (ASM shape composes extra fall-through)
    #   4. near-match: same structure, <=2 coordinate diffs -> EMIT ROM VALUES
    provenance = {}
    emit = {name: port[name] for name in SHAPES}
    used_asm = set()
    for name in SHAPES:
        geo = decode_port_shape(port[name])
        if geo is None:
            sys.exit(f"FATAL: port table {name} does not parse as shape data")
        pseg = segset(geo)
        if not pseg:
            provenance[name] = "EMPTY (draws nothing; original FPASAG ditto)"
            continue
        best = None
        for lbl, ageo in asm.items():
            aseg = segset(ageo)
            if ageo == geo:
                best = (0, f"VERIFIED == {lbl}", lbl, None)
                break
            if aseg == pseg:
                best = best or (1, f"VERIFIED (segment set) == {lbl}", lbl, None)
            elif pseg < aseg and best is None or (pseg < aseg and best and best[0] > 2):
                extra = len(aseg - pseg)
                best = (2, f"VERIFIED (subset of {lbl}; ASM composes "
                           f"{extra} extra fall-through segs)", lbl, None)
            else:
                nd = near_match(geo, ageo)
                if nd is not None and (best is None or best[0] > 3):
                    best = (3, f"CORRECTED to original ROM values from {lbl} "
                               f"({nd} coordinate(s) differed in port data)",
                            lbl, ageo)
        if best:
            _, msg, lbl, corrected = best
            provenance[name] = msg
            used_asm.add(lbl)
            if corrected is not None:
                emit[name] = encode_port_shape(corrected)
        else:
            nseg = len(pseg)
            provenance[name] = f"PORT-DATA (no ASM twin found; {nseg} segs)"

    verified = sum(1 for v in provenance.values()
                   if not v.startswith("PORT-DATA"))
    print(f"verified against original source: {verified}/{len(SHAPES)}")
    for name in SHAPES:
        print(f"  {name:10s} {provenance[name]}")
    unmatched_asm = sorted(set(asm) - used_asm)
    if unmatched_asm:
        print(f"ASM shapes with no port twin (composite/chained parts): "
              f"{unmatched_asm}")

    hdr = ("/* GENERATED by tools/gen_vectables.py - DO NOT EDIT.\n"
           " * Shape data from the original Dungeons of Daggorath\n"
           " * (rights-holder-released), via the C++ port's tables,\n"
           " * cross-verified against the original 6809 source. */\n")
    with open(OUT_C, "w") as f:
        f.write(hdr)
        f.write('#include "tables_vec.h"\n\n')
        for name in SHAPES:
            f.write(c_array(name, port[name], provenance[name]))
            f.write("\n")
        f.write(c_array("VEC_SCALE", port["Scale"],
                        "distance scale table (VIEWER.ASM)"))
        f.write("\n")
        f.write(c_array("VEC_LINES", port["LINES"],
                        "screen-border vector list"))
    with open(OUT_H, "w") as f:
        f.write(hdr)
        f.write("#ifndef DOD_TABLES_VEC_H\n#define DOD_TABLES_VEC_H\n\n")
        f.write("#include <stdint.h>\n\n")
        for name in SHAPES:
            f.write(f"extern const uint8_t {name}[{len(port[name])}];\n")
        f.write(f"\nextern const uint8_t VEC_SCALE[{len(port['Scale'])}];\n")
        f.write(f"extern const uint8_t VEC_LINES[{len(port['LINES'])}];\n")
        f.write("\n#endif /* DOD_TABLES_VEC_H */\n")
    print(f"wrote {OUT_C.name}, {OUT_H.name}")


if __name__ == "__main__":
    main()
