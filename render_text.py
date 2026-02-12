"""Parse ANSI output from screenshot.exe and render to a virtual 80x40 text grid."""
import re, subprocess, sys

raw = subprocess.check_output(["./screenshot.exe"], cwd=r"C:\Users\pixni\Desktop\roguelike").decode("utf-8", errors="replace")

# Split on our separator markers
sections = re.split(r'--- (.+?) ---', raw)

def render_section(data, width=80, height=35):
    grid = [[' ']*width for _ in range(height)]
    cx, cy = 0, 0

    i = 0
    while i < len(data):
        if data[i] == '\033' and i+1 < len(data) and data[i+1] == '[':
            # Parse ANSI escape
            j = i + 2
            while j < len(data) and data[j] not in 'ABCDHJKfhilmnpsu':
                j += 1
            if j < len(data):
                code = data[i+2:j]
                cmd = data[j]
                if cmd == 'H':  # cursor position
                    parts = code.split(';') if code else ['1','1']
                    if len(parts) >= 2:
                        cy = int(parts[0]) - 1 if parts[0] else 0
                        cx = int(parts[1]) - 1 if parts[1] else 0
                    elif len(parts) == 1 and parts[0]:
                        cy = int(parts[0]) - 1
                        cx = 0
                    else:
                        cy, cx = 0, 0
                elif cmd == 'J':  # clear screen
                    grid = [[' ']*width for _ in range(height)]
                # Skip color codes (m), etc.
                i = j + 1
                continue
        elif data[i] == '\n':
            cy += 1
            cx = 0
        elif data[i] == '\r':
            cx = 0
        else:
            if 0 <= cy < height and 0 <= cx < width:
                grid[cy][cx] = data[i]
                cx += 1
        i += 1

    # Find used rows
    lines = [''.join(row).rstrip() for row in grid]
    # Trim trailing empty lines
    while lines and not lines[-1]:
        lines.pop()
    return '\n'.join(lines)

# Render each section
for idx in range(0, len(sections)):
    s = sections[idx]
    if not s.strip():
        continue
    # Check if this is a label
    if idx > 0 and idx % 2 == 1:
        print(f"\n{'='*60}")
        print(f"  {s.strip()}")
        print(f"{'='*60}")
        continue
    result = render_section(s)
    if result.strip():
        print(result)
