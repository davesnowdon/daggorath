#!/usr/bin/env python3
"""Regenerate the scenario .keys files (jiffy-stamped ASCII for the
desktop shim's --replay).  Format: one "<jiffy> <ascii>" pair per line;
the shim's fscanf stops at the first non-numeric byte, so the files
carry no comments - this generator is the readable source of truth.

A command string is injected with every character on the same jiffy
(the parser's ring buffer is 32 deep); '|' denotes ENTER.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))


def emit(name, events):
    lines = []
    for jiffy, text in events:
        for ch in text:
            code = 13 if ch == '|' else ord(ch)
            lines.append(f"{jiffy} {code}\n")
    with open(os.path.join(HERE, name), "w") as f:
        f.writelines(lines)
    print(f"{name}: {len(lines)} keys")


# s2: the classic opening on a real game (abort the title fade), then a
# sweep of the command surface: torch, examine, movement, both-hand
# attacks, stow/pull, ZSAVE/ZLOAD round trip (needs --save-file), and
# the error paths (bad verb, bad noun, climb with no ladder, incant).
emit("s2-opening.keys", [
    (100,  "|"),          # any key aborts FADE_BEGIN -> real game
    (700,  "P R T|"),     # PULL RIGHT TORCH
    (850,  "U R|"),       # USE RIGHT (light it)
    (1000, "P L SW|"),    # PULL LEFT SWORD
    (1150, "E|"),         # EXAMINE
    (1300, "L|"),         # LOOK (back to 3D)
    (1450, "M|"),         # MOVE
    (1650, "M|"),
    (1850, "T L|"),       # TURN LEFT
    (2000, "M|"),
    (2200, "A R|"),       # ATTACK RIGHT (torch)
    (2400, "A L|"),       # ATTACK LEFT (sword)
    (2600, "S L|"),       # STOW LEFT
    (2750, "P L SW|"),    # PULL it back
    (2830, "ZL A|"),      # ZLOAD before any save -> CMDERR (no file)
    (2900, "ZS A|"),      # ZSAVE
    (3100, "ZL A|"),      # ZLOAD (LOAD_ABANDON path)
    (3300, "M|"),
    (3500, "Q|"),         # bad verb -> CMDERR
    (3650, "P X Y|"),     # bad noun -> CMDERR
    (3800, "C U|"),       # CLIMB UP
    (3950, "C D|"),       # CLIMB DOWN
    (4100, "I FIRE|"),    # INCANT without the ring
    (4250, "R R|"),       # REVEAL RIGHT
    (4400, "D R|"),       # DROP RIGHT (empty hand) -> CMDERR
    (4550, "G R T|"),     # GET RIGHT TORCH (nothing on floor) -> CMDERR
    (4700, "M B|"),       # MOVE BACK
    (4850, "D L|"),       # DROP LEFT: sword to the floor (success)
    (5000, "G L SW|"),    # GET LEFT SWORD back (success)
    (5150, "S R|"),       # STOW empty right hand -> CMDERR
    (5300, "U L|"),       # USE the sword (not usable)
])

# s3: combat soak - light up, arm, then hold position attacking on a
# cycle while the level-1 creatures close in (their movement, attacks
# on the player, heart-rate changes and possible faint/death are all
# deterministic under --turbo).
events = [
    (100,  "|"),
    (700,  "P R T|"),
    (850,  "U R|"),
    (1000, "P L SW|"),
]
jiffy = 1400
for cycle in range(60):
    events.append((jiffy, "A L|"))
    events.append((jiffy + 150, "A R|"))
    if cycle % 4 == 3:
        events.append((jiffy + 300, "T A|"))   # TURN AROUND
    jiffy += 450
events.append((34000, "|"))   # dismiss the death fade -> game_Restart
events.append((35500, "M|"))  # prove the restarted game accepts input
emit("s3-combat.keys", events)

# s4: descend to level 2.  The route from the fixed start (16,11) facing
# north to the hole at (15,4) was BFS'd from the generated maze
# (tools/print_maze.c raw mode); CLIMB DOWN there lands on level 2,
# which renders in INVERSE video (odd level parity) - covering PCLIMB's
# success path, mid-game NEWLVL and the inverted draw pipeline.
ROUTE = ["T A|", "M|", "T R|", "M|", "M|", "M|", "M|", "M|", "M|", "M|",
         "T R|", "M|", "M|"]
events = [
    (100, "|"),
    (500, "P R T|"),           # light up: creatures will converge, so
    (650, "U R|"),             # the march below wastes no time
]
jiffy = 850
for cmd in ROUTE:
    events.append((jiffy, cmd))
    jiffy += 100
events += [
    (jiffy + 80,   "C D|"),    # through the hole: level 2, inverse video
    (jiffy + 600,  "P L SW|"), # arm up on the new level
    (jiffy + 800,  "E|"),      # examine screen, inverted
    (jiffy + 1000, "L|"),
    (jiffy + 1200, "M|"),
    (jiffy + 1400, "A R|"),    # bare-fisted swing (EMPHND)
    (jiffy + 1600, "T L|"),
    (jiffy + 1800, "M|"),
]
emit("s4-descent.keys", events)

print("done")
