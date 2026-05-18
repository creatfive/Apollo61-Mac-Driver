with open('all_set_reports.bin', 'rb') as f:
    data = f.read()

def write_chunk(name, start_idx, num_pages):
    chunk = data[start_idx*64 : (start_idx + num_pages)*64]
    with open(name, 'wb') as out:
        out.write(chunk)
    print(f"Wrote {num_pages} pages to {name}")

write_chunk('payload_0415.bin', 2, 8)
write_chunk('payload_0411.bin', 13, 10)
write_chunk('payload_0413_1.bin', 28, 18)
write_chunk('payload_0417.bin', 50, 1)
write_chunk('payload_0413_2.bin', 54, 18)
write_chunk('payload_0413_3.bin', 76, 18)
