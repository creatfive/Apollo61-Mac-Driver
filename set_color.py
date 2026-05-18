#!/usr/bin/env python3
import sys
import os
import subprocess

if len(sys.argv) != 8:
    print("Usage: ./set_color.py <Mode> <R> <G> <B> <Brightness> <Speed> <Is_Rainbow_0_or_1>")
    sys.exit(1)

mode = int(sys.argv[1], 16)
r = int(sys.argv[2], 16)
g = int(sys.argv[3], 16)
b = int(sys.argv[4], 16)
brightness = int(sys.argv[5], 16)
speed = int(sys.argv[6], 16)
is_rainbow = int(sys.argv[7])

# Some firmwares treat specific pure magenta or pure white combinations as "rainbow cycle"
# slightly offsetting from pure 0xff bypasses the hardcoded rainbow trigger
if r == 0xff and g == 0x00 and b == 0xff:
    r = 0xfe
    b = 0xfe

subprocess.run(["python3", "split_payloads.py"], check=True, stdout=subprocess.DEVNULL)

def update_color_in_file(filename):
    if not os.path.exists(filename):
        return

    with open(filename, 'rb') as f:
        data = bytearray(f.read())
    
    # Mode configurations start at offset 0, each is 16 bytes.
    mode_index = mode - 1
    offset = mode_index * 16
    
    # Overwrite the active mode (Page 18)
    # Byte 8: 0x00 forces the keyboard to use the custom RGB color.
    # 0x01 forces it into 'Rainbow/Color Cycle' mode.
    byte_8 = 0x01 if is_rainbow == 1 else 0x00
    
    if 0 <= offset < 1088:
        data[offset + 1] = r
        data[offset + 2] = g
        data[offset + 3] = b
        data[offset + 8] = byte_8
        data[offset + 9] = brightness
        data[offset + 10] = speed
    
    active_mode_bytes = [mode, r, g, b, 0x00, 0x00, 0x00, 0x00, byte_8, brightness, speed, 0x00, 0x00, 0x00, 0xaa, 0x55]
    for i in range(16):
        data[1088 + i] = active_mode_bytes[i]

    with open(filename, 'wb') as f:
        f.write(data)
    print(f"Updated {filename} to Mode {mode:02x} RGB({r:02x}, {g:02x}, {b:02x}) B:{brightness} S:{speed} Rainbow:{is_rainbow}")

update_color_in_file('payload_0413_1.bin')
update_color_in_file('payload_0413_2.bin')
update_color_in_file('payload_0413_3.bin')

print("Pushing to keyboard...")
subprocess.run(["./apply_lighting.sh"])
