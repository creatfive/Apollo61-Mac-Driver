with open('all_set_reports.bin', 'rb') as f:
    data = f.read()

for i in range(0, len(data), 64):
    chunk = data[i:i+64]
    hex_str = ' '.join(f'{b:02x}' for b in chunk)
    print(f"Report {i//64:2d}: {hex_str[:24]} ... {hex_str[-14:]}")
