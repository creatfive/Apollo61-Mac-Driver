#!/bin/bash

echo "Applying captured keyboard configuration (Static Red)..."

cd mac-hid-tools || exit 1

echo "1. Sending 04 19 + 04 15 (Setup)"
./apollo61_probe_bulk --file ../payload_0415.bin --cmd 0x15 --write --i-understand-this-writes
sleep 2

echo "2. Sending 04 11 (Profiles)"
./apollo61_probe_bulk --file ../payload_0411.bin --cmd 0x11 --start --f0 --write --i-understand-this-writes
sleep 2

echo "3. Sending 04 13 (Lighting Part 1)"
./apollo61_probe_bulk --file ../payload_0413_1.bin --cmd 0x13 --start --f0 --write --i-understand-this-writes
sleep 2

echo "4. Sending 04 17 (Gaming Mode State)"
./apollo61_probe_bulk --file ../payload_0417.bin --cmd 0x17 --start --write --i-understand-this-writes
sleep 2

echo "5. Sending 04 13 (Lighting Part 2)"
./apollo61_probe_bulk --file ../payload_0413_2.bin --cmd 0x13 --start --f0 --write --i-understand-this-writes
sleep 1

echo "6. Sending 04 13 (Lighting Part 3)"
./apollo61_probe_bulk --file ../payload_0413_3.bin --cmd 0x13 --start --f0 --write --i-understand-this-writes

echo "Done! The keyboard should now be updated."
