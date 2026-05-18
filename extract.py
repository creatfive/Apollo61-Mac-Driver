import re
import binascii

out = open('all_set_reports.bin', 'wb')

with open('capture.txt', 'r') as f:
    lines = f.readlines()

current_hex = ""
for line in lines:
    if line.startswith('\t0x'):
        # Extract the hex part
        hex_part = line[10:50].replace(' ', '')
        current_hex += hex_part
    else:
        if current_hex:
            # We finished reading a packet. Let's look for SET_REPORT
            # 2109000300004000
            idx = current_hex.find("2109000300004000")
            if idx != -1:
                payload_hex = current_hex[idx+16:idx+16+128]
                if len(payload_hex) == 128:
                    out.write(binascii.unhexlify(payload_hex))
        current_hex = ""

# Handle last packet
if current_hex:
    idx = current_hex.find("2109000300004000")
    if idx != -1:
        payload_hex = current_hex[idx+16:idx+16+128]
        if len(payload_hex) == 128:
            out.write(binascii.unhexlify(payload_hex))

out.close()
