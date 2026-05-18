#!/usr/bin/env python3
import sys
import os
import subprocess

if len(sys.argv) != 4:
    print("Usage: ./set_color.py <R> <G> <B>")
    sys.exit(1)

r = int(sys.argv[1], 16)
g = int(sys.argv[2], 16)
b = int(sys.argv[3], 16)

# Some firmwares treat specific pure magenta or pure white combinations as "rainbow cycle"
# If we want a solid color, slightly offsetting from pure 0xff can bypass the hardcoded rainbow trigger
if r == 0xff and g == 0x00 and b == 0xff:
    r = 0xfe
    b = 0xfe

subprocess.run(["python3", "split_payloads.py"], check=True, stdout=subprocess.DEVNULL)

def update_color_in_file(filename):
    if not os.path.exists(filename):
        return

    with open(filename, 'rb') as f:
        data = bytearray(f.read())
    
    # Mode 1 (Static) is at offset 0
    data[1] = r
    data[2] = g
    data[3] = b

    # Overwrite the active mode (Page 18) to expressly be Mode 01 (Static)
    # Format: [01] [R] [G] [B] [00] [00] [00] [00] [00] [0f] [0a] [00] [00] [00] [aa] [55]
    active_mode_bytes = [0x01, r, g, b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0a, 0x00, 0x00, 0x00, 0xaa, 0x55]
    for i in range(16):
        data[1088 + i] = active_mode_bytes[i]

    with open(filename, 'wb') as f:
        f.write(data)
    print(f"Updated {filename} to Static Mode RGB({r:02x}, {g:02x}, {b:02x})")

update_color_in_file('payload_0413_1.bin')
update_color_in_file('payload_0413_2.bin')
update_color_in_file('payload_0413_3.bin')

print("Pushing to keyboard...")
subprocess.run(["./apply_lighting.sh"])
