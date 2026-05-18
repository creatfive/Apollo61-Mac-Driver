wwwwwwwwwwwwwwwwwwwwwwwwwwwwwiesewwewewewewewwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwjhghjghjghjghghhgghghjgljkkljlkjkljkljlkjjjjkjkjkjkjlkjkljljjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjhjjjjhjjhjhjhjhjhjhjhjhjhjhjjjjjjjjjjjwioiiiiiiwwwwwwwwwwwwwwjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwhgjhghjghjghjghjghghjghjghjghjghjghhhhggggggggggggghjkglkjkljkljkljkljkljkljkljkljkljkljkljljklhhhhhjjjjjjhhjjhhjhhjjhhjjhhj# APOLLO61 macOS HID tools

This folder starts a native macOS path for the APOLLO61 / 61-key keyboard
software. The Windows installer cannot be converted directly, so these tools
talk to the keyboard through macOS IOKit HID APIs.

## Build

```sh
make
```

## Scan for the keyboard

The Windows config targets USB VID `05AC` and PID `024F`. Scan for that device:

```sh
./apollo61_hid_scan
```

Scan every HID device:

```sh
./apollo61_hid_scan --all
```

Scan a specific VID/PID:

```sh
./apollo61_hid_scan --vid 05ac --pid 024f
```

## Monitor input reports

Watch raw input reports for 20 seconds:

```sh
./apollo61_hid_monitor
```

Watch only the vendor-looking report ID discovered by the scanner:

```sh
./apollo61_hid_monitor --report 177
```

## Read feature report

In wired USB mode, the APOLLO61 exposes a 64-byte feature report on the keyboard
interface. Read it without writing anything:

```sh
./apollo61_hid_get_feature
```

## Prepare a feature write

The Windows app uses `HidD_SetFeature(..., 0x41)`, which maps to report ID `0`
plus a 64-byte payload on macOS. The write helper is dry-run by default:

```sh
./apollo61_hid_set_feature --payload "04 18"
```

To actually send a packet, both safety flags are required:

```sh
./apollo61_hid_set_feature --payload "04 18" --write --i-understand-this-writes
```

Do not send guessed packets casually. Some commands appear to change persistent
keyboard state.

## Probe the handshake

Run the known start/probe/end sequence in one command. Dry-run first:

```sh
./apollo61_probe_handshake
```

Send the sequence and decode the response status bytes:

```sh
./apollo61_probe_handshake --write --i-understand-this-writes
```

Probe one command with optional start/end wrapping:

```sh
./apollo61_probe_command --payload "04 13 00 00 00 00 00 00 12" --start --end
```

To actually send it:

```sh
./apollo61_probe_command --payload "04 13 00 00 00 00 00 00 12" --start --end --write --i-understand-this-writes
```

## Probe the bulk transfer

Send a bulk transfer of 64-byte payload pages (such as lighting blob data):

```sh
./apollo61_probe_bulk --file payload.bin
```

To actually execute the bulk transfer:

```sh
./apollo61_probe_bulk --file payload.bin --write --i-understand-this-writes
```

## Probe for reading data

Try to coax the keyboard into dumping its current lighting or profile state:

```sh
./apollo61_probe_read --cmd 04 89
```

To actually execute the read sequence:

```sh
./apollo61_probe_read --cmd 04 89 --write --i-understand-this-writes
```

## Next step

The next task is to map the Windows command payloads to features like lighting,
profiles, and key remapping. Start with dry-run packet construction, then only
send clearly identified commands.
